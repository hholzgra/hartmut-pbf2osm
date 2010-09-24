#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zlib.h>

#define OUR_TOOL "pbf2osm"
#define NANO_DEGREE .000000001
#define MAX_BLOCK_HEADER_SIZE	64*1024
//#define MAX_BLOB_SIZE		32*1024*1024

/* 
 * (Inline) function to convert a number of seconds since the epoch to
 * a timestamp.
 */
char * deltatime2timestamp(const long int deltatimestamp) {
  /* TODO: add date_granularity */
  struct tm *ts = gmtime(&deltatimestamp);
  if (ts == NULL) {
    return NULL;
  }
  
  // maybe allocate this on the stack permanently?
  char *timestamp = (char *)malloc(21 * sizeof(char));
  strftime(timestamp, 21, "%Y-%m-%dT%H:%M:%SZ" , ts);
  return timestamp;
}

unsigned char * handleCompressedBlob (Blob *bmsg) {
	if (bmsg->has_zlib_data) {
		int ret;
		unsigned char *uncompressed;
		z_stream strm;
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = bmsg->zlib_data.len;
		strm.next_in = bmsg->zlib_data.data;
		strm.avail_out = bmsg->raw_size;
		uncompressed = (unsigned char *) malloc(bmsg->raw_size * sizeof(char));
		if (uncompressed == NULL) {
			fprintf(stderr, "Error allocating the decompression buffer\n");
			return NULL;
		}
		strm.next_out = uncompressed;

		ret = inflateInit(&strm);
		if (ret != Z_OK) {
			fprintf(stderr, "Zlib init failed\n");
			return NULL;
		}

		ret = inflate(&strm, Z_NO_FLUSH);
			
		(void)inflateEnd(&strm);
		
		if (ret != Z_STREAM_END) {
			fprintf(stderr, "Zlib compression failed\n");
			return NULL;
		}

		return uncompressed;
	} else
	if (bmsg->has_lzma_data) {
		fprintf(stderr, "LZMA data\n");
	} else
	if (bmsg->has_bzip2_data) {
		fprintf(stderr, "bzip2 data\n");
	} else
	{
		fprintf(stderr, "We cannot handle the %d non-raw bytes yet...\n", bmsg->raw_size);
		return NULL;
	}

	return NULL;
}

int main(int argc, char **argv) {
	uint32_t length;	 			// length of the BlockHeader message
	BlockHeader *bhmsg = NULL;			// serialized BlockHeader message
	Blob *bmsg = NULL;				// serialized Blob message (size is given in the header)


	char lenbuf[4];
	unsigned char *bhbuf = NULL;
	unsigned char *bbuf = NULL;

	unsigned char c;

	unsigned int i;

	enum {
		osmheader,
		osmdata
	} state;

	puts("<?xml version='1.0' encoding='UTF-8'?>");
	puts("<osm version=\"0.6\" generator=\"" OUR_TOOL "\">");

	do {
	/* First we are going to receive the size of the BlockHeader */
	for (i = 0; i < 4 && (c=fgetc(stdin)) != EOF; i++) {
		lenbuf[i] = c;
	}

	length = ntohl(*((uint32_t *) lenbuf));		// convert the buffer to a value

	fprintf(stderr, "Length BlockHeader: %d\n", length);

	if (length <= 0 || length > MAX_BLOCK_HEADER_SIZE) {
		fprintf(stderr, "Block Header isn't present or exceeds maximum size\n");
		return 1;
	}

	
	/* Since we know the length of the BlockHeader now, we can allocate it */
	bhbuf =  (unsigned char *) malloc(length * sizeof(char));
	if (bhbuf == NULL) {
		fprintf (stderr, "Error allocating BlockHeader buffer\n");
		return 1;
	}

	/* We are reading the BlockHeader */
	for (i = 0; i < length && (c=fgetc(stdin)) != EOF; i++) {
		bhbuf[i] = c;
	}

	bhmsg = block_header__unpack (NULL, i, bhbuf);
	free(bhbuf);
	if (bhmsg == NULL) {
		fprintf(stderr, "Error unpacking BlockHeader message\n");
		return 1;
	}

	length = bhmsg->datasize;
	fprintf(stderr, "Type: %s\nLength: %u\n", bhmsg->type, length);

	if (strcmp(bhmsg->type, "OSMHeader") == 0) {
		state = osmheader;
	} else if (strcmp(bhmsg->type, "OSMData") == 0) {
		state = osmdata;
	}

	block_header__free_unpacked (bhmsg, &protobuf_c_system_allocator);

	/* We are now reading the 'Blob' */
	bbuf = (unsigned char *) malloc(length * sizeof(char *));
	for (i = 0; i < length && (c=fgetc(stdin)) != EOF; i++) {
		bbuf[i] = c;
	}

	bmsg = blob__unpack (NULL, i, bbuf);
	if (bmsg == NULL) {
		fprintf(stderr, "Error unpacking Blob message\n");
		return 1;
	}
	free(bbuf);
	
	unsigned char *uncompressed;
	if (bmsg->has_raw) {
		uncompressed = bmsg->raw.data;
	} else {
		uncompressed = handleCompressedBlob(bmsg);
		if (uncompressed == NULL) {
			fprintf(stderr, "Uncompression failed\n");
			return 1;
		}
	}

	if (state == osmheader) {
		HeaderBlock *hmsg = header_block__unpack (NULL, bmsg->raw_size, uncompressed);
		if (hmsg == NULL) {
			fprintf(stderr, "Error unpacking HeaderBlock message\n");
			return 1;
		}

		fprintf(stderr, "%s\n", hmsg->required_features[0]);

		header_block__free_unpacked (hmsg, &protobuf_c_system_allocator);
	} else if (state == osmdata) {
		unsigned int j = 0;
		double lat_offset, lon_offset, granularity;

		PrimitiveBlock *pmsg = primitive_block__unpack (NULL, bmsg->raw_size, uncompressed);
		if (pmsg == NULL) {
			fprintf(stderr, "Error unpacking PrimitiveBlock message\n");
			return 1;
		}

		lat_offset = NANO_DEGREE * pmsg->lat_offset;
		lon_offset = NANO_DEGREE * pmsg->lon_offset;
		granularity = NANO_DEGREE * pmsg->granularity;

		fprintf(stderr, "\t""Granularity: %d""\n", pmsg->granularity);
		fprintf(stderr, "\t""Primitive groups: %li""\n", pmsg->n_primitivegroup);
		for (j = 0; j < pmsg->n_primitivegroup; j++) {
			fprintf(stderr,"\t\t""Nodes: %li""\n"\
			       "\t\t""Ways: %li""\n"\
			       "\t\t""Relations: %li""\n",
			       (pmsg->primitivegroup[j]->dense ?
					pmsg->primitivegroup[j]->dense->n_id :
			       		pmsg->primitivegroup[j]->n_nodes),
			       pmsg->primitivegroup[j]->n_ways,
			       pmsg->primitivegroup[j]->n_relations);

			/* TODO: Nodes is *untested* */
			if (pmsg->primitivegroup[j]->n_nodes > 0) {
				int k;
				for (k = 0; k < pmsg->primitivegroup[j]->n_nodes; k++) {
					Node *node = pmsg->primitivegroup[j]->nodes[k];

					printf("\t""<node id=\"%li\" lat=\"%.07f\" lon=\"%.07f\"",
						node->id,
						lat_offset + (node->lat * granularity),
						lon_offset + (node->lon * granularity));
					if (node->info) {
						Info *info = node->info;
						if (info->has_version) {
							printf(" version=\"%d\"", (int) info->version);
						}
						if (info->has_changeset) {
							printf(" changeset=\"%d\"", (int) info->changeset);
						}
						if (info->has_user_sid) {
							ProtobufCBinaryData user = pmsg->stringtable->s[info->user_sid];
							printf(" user=\"%.*s\"", (int) user.len, user.data);
						}
						if (info->has_uid) {
							printf(" uid\"%d\"", (int) info->uid);
						}
						if (info->has_timestamp) {
							char *timestamp = deltatime2timestamp(info->timestamp * (pmsg->date_granularity / 1000));
							printf(" timestamp=\"%s\"", timestamp);
							free(timestamp);
						}
					}

					if (node->n_keys == 0 || node->n_vals == 0) {
						puts(" />");
					} else {
						int l;
						
						puts(">");

						for (l = 0; l < node->n_keys; l++) {
							ProtobufCBinaryData key = pmsg->stringtable->s[node->keys[l]];
							ProtobufCBinaryData val = pmsg->stringtable->s[node->vals[l]];

							printf ("\t\t""<tag k=\"%.*s\" v=\"%.*s\" />""\n",
								(int) key.len, key.data, 
								(int) val.len, val.data);
						}

						puts("\t""</node>");
					}
				}
			}
			/* else // currently the protocol generators only have one type per primitive block */
			if (pmsg->primitivegroup[j]->n_ways > 0) {
				int k;
				for (k = 0; k < pmsg->primitivegroup[j]->n_ways; k++) {
					Way *way = pmsg->primitivegroup[j]->ways[k];
					printf("\t""<way id=\"%li\"", way->id);
					if (way->info) {
						Info *info = way->info;
						if (info->has_version) {
							printf(" version=\"%d\"", (int) info->version);
						}
						if (info->has_changeset) {
							printf(" changeset=\"%d\"", (int) info->changeset);
						}
						if (info->has_user_sid) {
							ProtobufCBinaryData user = pmsg->stringtable->s[info->user_sid];
							printf(" user=\"%.*s\"", (int) user.len, user.data);
						}
						if (info->has_uid) {
							printf(" uid=\"%d\"", (int) info->uid);
						}
						if (info->has_timestamp) {
							char *timestamp = deltatime2timestamp(info->timestamp * (pmsg->date_granularity / 1000));
							printf(" timestamp=\"%s\"", timestamp);
							free(timestamp);
						}
					}

					if ((way->n_keys == 0 || way->n_vals == 0) && way->n_refs == 0) {
						puts("/>");
					} else {
						int l;
						long int deltaref = 0;
						
						puts(">");
						
						for (l = 0; l < way->n_refs; l++) {
							deltaref += way->refs[l];
							printf ("\t\t""<nd ref=\"%li\"/>""\n", deltaref);
						}

						for (l = 0; l < way->n_keys; l++) {
							ProtobufCBinaryData key = pmsg->stringtable->s[way->keys[l]];
							ProtobufCBinaryData val = pmsg->stringtable->s[way->vals[l]];

							printf ("\t\t""<tag k=\"%.*s\" v=\"%.*s\"/>""\n",
								(int) key.len, key.data, 
								(int) val.len, val.data);
						}

						puts("\t""</way>");
					}
				}
			}
			/* else // currently the protocol generators only have one type per primitive block */
			if (pmsg->primitivegroup[j]->n_relations > 0) {
				int k;
				for (k = 0; k < pmsg->primitivegroup[j]->n_relations; k++) {
					Relation *relation = pmsg->primitivegroup[j]->relations[k];
					printf("<relation id=\"%li\"", relation->id);
					if (relation->info) {
						Info *info = relation->info;
						if (info->has_version) {
							printf(" version=\"%d\"", (int) info->version);
						}
						if (info->has_changeset) {
							printf(" changeset=\"%d\"", (int) info->changeset);
						}
						if (info->has_user_sid) {
							ProtobufCBinaryData user = pmsg->stringtable->s[info->user_sid];
							printf(" user=\"%.*s\"", (int) user.len, user.data);
						}
						if (info->has_uid) {
							printf(" uid=\"%d\"", (int) info->uid);
						}
						if (info->has_timestamp) {
							char *timestamp = deltatime2timestamp(info->timestamp * (pmsg->date_granularity / 1000));
							printf(" timestamp=\"%s\"", timestamp);
							free(timestamp);
						}
					}

					if ((relation->n_keys == 0 || relation->n_vals == 0) && relation->n_memids == 0) {
						puts("/>");
					} else {
						int l;
						long int deltamemids = 0;
						
						puts(">");
						
						for (l = 0; l < relation->n_memids; l++) {
							char *type;
							ProtobufCBinaryData role =  pmsg->stringtable->s[relation->roles_sid[l]];
							deltamemids += relation->memids[l];

							switch (relation->types[l]) {
								case RELATION__MEMBER_TYPE__NODE:
									type = "node";
									break;
								case RELATION__MEMBER_TYPE__WAY:
									type = "way";
									break;
								case RELATION__MEMBER_TYPE__RELATION:
									type = "relation";
									break;
								default:
									fprintf(stderr, "Unsupported type: %u""\n", relation->types[l]);
									return 1;
							}
							printf ("\t\t""<member type=\"%s\" ref=\"%li\" role=\"%.*s\"/>""\n",
									type, deltamemids, (int) role.len, role.data);
						}

						for (l = 0; l < relation->n_keys; l++) {
							ProtobufCBinaryData key = pmsg->stringtable->s[relation->keys[l]];
							ProtobufCBinaryData val = pmsg->stringtable->s[relation->vals[l]];

							printf ("\t\t""<tag k=\"%.*s\" v=\"%.*s\"/>""\n",
								(int) key.len, key.data, 
								(int) val.len, val.data);
						}

						puts("</relation>");
					}
				}
			}
			/* else // currently the protocol generators only have one type per primitive block */
			if (pmsg->primitivegroup[j]->dense) {
				int k, l = 0;
				unsigned long int deltaid = 0;
				long int deltalat = 0;
				long int deltalon = 0;
				long int deltatimestamp = 0;
				long int deltachangeset = 0;
				long int deltauid = 0;
				long int deltauser_sid = 0;

				DenseNodes *dense = pmsg->primitivegroup[j]->dense;
				

				for (k = 0; k < dense->n_id; k++) {
					short has_tags = 0;
					deltaid += dense->id[k];
					deltalat += dense->lat[k];
					deltalon += dense->lon[k];
					
					printf("\t""<node id=\"%li\" lat=\"%.07f\" lon=\"%.07f\"", deltaid,
							lat_offset + (deltalat * granularity),
							lon_offset + (deltalon * granularity));

					if (dense->denseinfo) {
						char *timestamp;
						DenseInfo *denseinfo = dense->denseinfo;

						deltatimestamp += denseinfo->timestamp[k];
						deltachangeset += denseinfo->changeset[k];
						deltauid += denseinfo->uid[k];
						deltauser_sid += denseinfo->user_sid[k];

						timestamp = deltatime2timestamp(deltatimestamp * (pmsg->date_granularity / 1000));

						printf(" version=\"%d\" changeset=\"%li\" user=\"%.*s\" uid=\"%li\" timestamp=\"%s\"",
							(int) denseinfo->version[k], deltachangeset,
							(int) pmsg->stringtable->s[deltauser_sid].len,
							pmsg->stringtable->s[deltauser_sid].data, deltauid, timestamp);

						free(timestamp);
					}

					if (l < dense->n_keys_vals) {
						while (dense->keys_vals[l] != 0 &&
						       l < dense->n_keys_vals) {
							if (has_tags == 0) {
								has_tags++;
								puts(">");
							}

						       	int m =  dense->keys_vals[l];
						       	int n =  dense->keys_vals[l+1];
							printf ("\t""<tag k=\"%.*s\" v=\"%.*s\"/>""\n",
								(int) pmsg->stringtable->s[m].len, 
								pmsg->stringtable->s[m].data, 
								(int) pmsg->stringtable->s[n].len,
								pmsg->stringtable->s[n].data);
							l += 2;
						}
						l += 1;
					}

					if (has_tags == 0) {
						puts("/>");
					} else {
						puts("\t""</node>");
					} 
				}
			}
		}

		primitive_block__free_unpacked (pmsg, &protobuf_c_system_allocator);
	}
	if (!bmsg->has_raw) free(uncompressed);
	blob__free_unpacked (bmsg, &protobuf_c_system_allocator);


	} while (c != EOF);

	puts("</osm>");
}
