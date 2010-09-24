#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"
#include <stdio.h>
#include <stdlib.h>

#include <zlib.h>

#define NANO_DEGREE .000000001f
#define MAX_BLOCK_HEADER_SIZE	64*1024
//#define MAX_BLOB_SIZE		32*1024*1024

char * handleCompressedBlob (Blob *bmsg) {
	if (bmsg->has_zlib_data) {
		int ret;
		char *uncompressed;
		z_stream strm;
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = bmsg->zlib_data.len;
		strm.next_in = bmsg->zlib_data.data;
		strm.avail_out = bmsg->raw_size;
		uncompressed = (char *) malloc(bmsg->raw_size * sizeof(char));
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
		printf("LZMA data\n");
	} else
	if (bmsg->has_bzip2_data) {
		printf("bzip2 data\n");
	} else
	{
		fprintf(stderr, "We cannot handle the %d non-raw bytes yet...\n", bmsg->raw_size);
		return NULL;
	}
}

int main(int argc, char **argv) {
	uint32_t length;	 			// length of the BlockHeader message
	BlockHeader *bhmsg = NULL;			// serialized BlockHeader message
	Blob *bmsg = NULL;				// serialized Blob message (size is given in the header)


	char lenbuf[4];
	char *bhbuf = NULL;
	char *bbuf = NULL;

	unsigned char c;

	unsigned int i;

	enum {
		osmheader,
		osmdata
	} state;

	do {
	/* First we are going to receive the size of the BlockHeader */
	for (i = 0; i < 4 && (c=fgetc(stdin)) != EOF; i++) {
		lenbuf[i] = c;
	}

	length = ntohl(*((uint32_t *) lenbuf));		// convert the buffer to a value

	printf("Length BlockHeader: %d\n", length);

	if (length <= 0 || length > MAX_BLOCK_HEADER_SIZE) {
		fprintf(stderr, "Block Header isn't present or exceeds maximum size\n");
		return 1;
	}

	
	/* Since we know the length of the BlockHeader now, we can allocate it */
	bhbuf =  (char *) malloc(length * sizeof(char));
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
	printf("Type: %s\nLength: %u\n", bhmsg->type, length);

	if (strcmp(bhmsg->type, "OSMHeader") == 0) {
		state = osmheader;
	} else if (strcmp(bhmsg->type, "OSMData") == 0) {
		state = osmdata;
	}

	block_header__free_unpacked (bhmsg, &protobuf_c_system_allocator);

	/* We are now reading the 'Blob' */
	bbuf = (char *) malloc(length * sizeof(char *));
	for (i = 0; i < length && (c=fgetc(stdin)) != EOF; i++) {
		bbuf[i] = c;
	}

	bmsg = blob__unpack (NULL, i, bbuf);
	if (bmsg == NULL) {
		fprintf(stderr, "Error unpacking Blob message\n");
		return 1;
	}
	free(bbuf);
	
	char *uncompressed;
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

		printf("%s\n", hmsg->required_features[0]);

		header_block__free_unpacked (hmsg, &protobuf_c_system_allocator);
	} else if (state == osmdata) {
		unsigned int j = 0;
		double granularity;
		PrimitiveBlock *pmsg = primitive_block__unpack (NULL, bmsg->raw_size, uncompressed);
		if (pmsg == NULL) {
			fprintf(stderr, "Error unpacking PrimitiveBlock message\n");
			return 1;
		}

		granularity = pmsg->granularity * NANO_DEGREE;
		printf("\t""Granularity: %li""\n", pmsg->granularity);
		printf("\t""Primitive groups: %li""\n", pmsg->n_primitivegroup);
		for (j = 0; j < pmsg->n_primitivegroup; j++) {
			printf("\t\t""Nodes: %li""\n"\
			       "\t\t""Ways: %li""\n"\
			       "\t\t""Relations: %li""\n",
			       (pmsg->primitivegroup[j]->dense ?
					pmsg->primitivegroup[j]->dense->n_id :
			       		pmsg->primitivegroup[j]->n_nodes),
			       pmsg->primitivegroup[j]->n_ways,
			       pmsg->primitivegroup[j]->n_relations);


			if (pmsg->primitivegroup[j]->dense) {
				int k, l = 0;
				unsigned long int deltaid = 0;
				long int deltalat = 0;
				long int deltalon = 0;
				long int deltaversion = 0;
				long int deltatimestamp = 0;
				long int deltachangeset = 0;
				long int deltauid = 0;
				long int deltauser_sid = 0;

				for (k = 0; k < pmsg->primitivegroup[j]->dense->n_id; k++) {
					short has_tags = 0;
					deltaid += pmsg->primitivegroup[j]->dense->id[k];
					deltalat += pmsg->primitivegroup[j]->dense->lat[k];
					deltalon += pmsg->primitivegroup[j]->dense->lon[k];
						
					printf("<node id=\"%li\" lat=\"%f\" lon=\"%f\"", deltaid, granularity*deltalat, granularity*deltalon);
					if (pmsg->primitivegroup[j]->dense->denseinfo) {
						deltaversion += pmsg->primitivegroup[j]->dense->denseinfo->version[k];
						deltatimestamp += pmsg->primitivegroup[j]->dense->denseinfo->timestamp[k];
						deltachangeset += pmsg->primitivegroup[j]->dense->denseinfo->changeset[k];
						deltauid += pmsg->primitivegroup[j]->dense->denseinfo->uid[k];
						deltauser_sid += pmsg->primitivegroup[j]->dense->denseinfo->user_sid[k];

						printf(" version=\"%li\" changeset=\"%li\" user=\"%.*s\" uid=\"%li\" timestamp=\"%li\"",
							deltaversion, deltachangeset,
							(int) pmsg->stringtable->s[deltauser_sid].len,
							pmsg->stringtable->s[deltauser_sid].data, deltauid, deltatimestamp);

					}

					if (l < pmsg->primitivegroup[j]->dense->n_keys_vals) {
						while (pmsg->primitivegroup[j]->dense->keys_vals[l] != 0 &&
						       l < pmsg->primitivegroup[j]->dense->n_keys_vals) {
							if (has_tags == 0) {
								has_tags = 1;
								puts(">");
							}

						       	int m =  pmsg->primitivegroup[j]->dense->keys_vals[l];
						       	int n =  pmsg->primitivegroup[j]->dense->keys_vals[l+1];
							printf ("\t""<tag k=\"%.*s\" v=\"%.*s\" />""\n",
								(int) pmsg->stringtable->s[m].len, 
								pmsg->stringtable->s[m].data, 
								(int) pmsg->stringtable->s[n].len,
								pmsg->stringtable->s[n].data);
							l += 2;
						}
						l += 1;
					}

					if (has_tags == 0) {
						puts(" />");
					} else {
						puts("</node>");
					} 
				}
			}
		}

		primitive_block__free_unpacked (pmsg, &protobuf_c_system_allocator);
	}
	if (!bmsg->has_raw) free(uncompressed);
	blob__free_unpacked (bmsg, &protobuf_c_system_allocator);


	} while (c != EOF);
}
