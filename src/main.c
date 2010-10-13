/* This file is licensed under the General Public License 3 or in your opinion any later version
 *
 * This file links to files generated by protobuf-c <http://code.google.com/p/protobuf-c/>
 * This file uses the itoa-function from Luk�s Chmela benchmarked at <http://www.strudel.org.uk/itoa/>
 *
 * Stefan de Konink <stefan at konink dot de>
 * Roeland Douma <contact at rullzer dot com>
 *
 */

#include "fileformat.pb-c.h"
#include "osmformat.pb-c.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <zlib.h>
#include <bzlib.h>

#define verbose 0
#define OUR_TOOL "pbf2osm"
#define NANO_DEGREE .000000001
#define MAX_BLOCK_HEADER_SIZE 64*1024
#define MAX_BLOB_SIZE 32*1024*1024

#define printtag(key,val) \
    fputs_unlocked ("\t\t""<tag k=\"", stdout); \
    printescape(key.data, key.len); \
    fputs_unlocked ("\" v=\"", stdout); \
    printescape(val.data, val.len); \
    fputs_unlocked ("\" />""\n", stdout);

#define printtimestamp(attribute, timestamp) \
    char tsbuf[21]; \
    deltatime2timestamp(timestamp * (pmsg->date_granularity / 1000), tsbuf); \
    fputs_unlocked(" "attribute"=\"", stdout); \
    fputs_unlocked(tsbuf, stdout); \
    fputc_unlocked('"', stdout);

#define printuser(user) \
    fputs_unlocked(" user=\"", stdout); \
    printescape(user.data, user.len); \
    fputc_unlocked('"', stdout);

#define printnumericattribute(attribute, value) \
    fputs_unlocked(" "attribute"=\"", stdout); \
    itoa(value); \
    fputc_unlocked('"', stdout);

#define printsotid(name, id) \
    fputs_unlocked("\t<"name" id=\"", stdout); \
    itoa(id); \
    fputc_unlocked('"', stdout);

#define printnd(ref) \
    fputs_unlocked("\t\t""<nd ref=\"", stdout); \
    itoa(ref); \
    fputs_unlocked("\"/>""\n", stdout);

#define printmember(type, ref, role); \
    fputs_unlocked("\t\t""<member type=\"", stdout); \
    fputs_unlocked(type, stdout); \
    fputs_unlocked("\" ref=\"", stdout); \
    itoa(ref); \
    fputs_unlocked("\" role=\"", stdout); \
    printescape(role.data, role.len); \
    fputs_unlocked("\"/>""\n", stdout);

/* 
 * (Inline) function to convert a number of seconds since the epoch to
 * a timestamp.
 */
void deltatime2timestamp(const long int deltatimestamp, char *timestamp) {
    struct tm *ts = gmtime(&deltatimestamp);
    if (ts == NULL) {
        timestamp[0] = '\0';
    }

    strftime(timestamp, 21, "%Y-%m-%dT%H:%M:%SZ" , ts);
}

void itoa(int value) {
    char result[20];
    unsigned char base = 10;
    // check that the base if valid
    // if (base < 2 || base > 36) { *result = '\0'; return result; }
    
    char* ptr = result, *ptr1 = result, tmp_char;
    int tmp_value;
    
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * base)];
    } while ( value );
    
    // Apply negative sign
    if (tmp_value < 0) *ptr++ = '-';
    *ptr-- = '\0';
    while(ptr1 < ptr) {
       tmp_char = *ptr;
       *ptr--= *ptr1;
       *ptr1++ = tmp_char;
    }
    
    fputs_unlocked(result, stdout);
}

void printescape(unsigned char *s, unsigned int l) {
    for (unsigned int i = 0; i < l; i++) {
        switch (s[i]) {
            case '<':
                fputs_unlocked("&#60;", stdout);
                continue;
            case '>':
                fputs_unlocked("&#62;", stdout);
                continue;
            case '&':
                fputs_unlocked("&#38;", stdout);
                continue;
            case '\'':
                fputs_unlocked("&#39;", stdout);
                continue;
            case '"':
                fputs_unlocked("&#34;", stdout);
                continue;
            case '\r':
                fputs_unlocked("&#13;", stdout);
                continue;
            case '\n':
                fputs_unlocked("&#10;", stdout);
                continue;
            case '\t':
                fputs_unlocked("&#9;", stdout);
                continue;
            default:
                fputc_unlocked(s[i], stdout);
        }
    }
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
        uncompressed = (unsigned char *) malloc(bmsg->raw_size * sizeof(unsigned char));
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
        int ret;
        char *uncompressed;
        bz_stream strm;
        strm.bzalloc = NULL;
        strm.bzfree = NULL;
        strm.opaque = NULL;
        strm.avail_in = bmsg->bzip2_data.len;
        strm.next_in = (char *) bmsg->bzip2_data.data;
        strm.avail_out = bmsg->raw_size;
        uncompressed = (char *) malloc(bmsg->raw_size * sizeof(char));
        if (uncompressed == NULL) {
            fprintf(stderr, "Error allocating the decompression buffer\n");
            return NULL;
        }
        strm.next_out = uncompressed;

        ret = BZ2_bzDecompressInit(&strm, 0, 0);
        if (ret != BZ_OK) {
            fprintf(stderr, "Bzip2 init failed\n");
            return NULL;
        }

        (void)BZ2_bzDecompressEnd(&strm);
        
        if (ret != BZ_STREAM_END) {
            fprintf(stderr, "Bzip2 compression failed\n");
            return NULL;
        }

        return (unsigned char *) uncompressed;
    } else
    {
        fprintf(stderr, "We cannot handle the %d non-raw bytes yet...\n", bmsg->raw_size);
        return NULL;
    }

    return NULL;
}


int main(int argc, char **argv) {
    uint32_t length;            // length of the BlockHeader message
    BlockHeader *bhmsg = NULL;  // serialized BlockHeader message
    Blob *bmsg = NULL;          // serialized Blob message (size is given in the header)


    char lenbuf[4];
    unsigned char *buf = NULL;

    unsigned char c;

    unsigned int i;

    enum {
        osmheader,
        osmdata
    } state = osmheader;

    fputs_unlocked("<?xml version='1.0' encoding='UTF-8'?>\n<osm version=\"0.6\" generator=\"" OUR_TOOL "\">""\n", stdout);

    do {
    /* First we are going to receive the size of the BlockHeader */
    for (i = 0; i < 4 && (c=fgetc(stdin)) != EOF; i++) {
        lenbuf[i] = c;
    }
    
    length = ntohl(*((uint32_t *) lenbuf));  // convert the buffer to a value

    if (verbose) fprintf(stderr, "Length BlockHeader: %d\n", length);

    if (length <= 0 || length > MAX_BLOCK_HEADER_SIZE) {
        if (length == -1) {
            fputs_unlocked("</osm>""\n", stdout);
            return 0;
        }

        fprintf(stderr, "Block Header isn't present or exceeds minimum/maximum size\n");
        return 1;
    }

    
    /* Since we know the length of the BlockHeader now, we can allocate it */
    buf = (unsigned char *) malloc(length * sizeof(unsigned char));
    if (buf == NULL) {
        fprintf (stderr, "Error allocating BlockHeader buffer\n");
        return 1;
    }

    /* We are reading the BlockHeader */
    for (i = 0; i < length && (c=fgetc(stdin)) != EOF; i++) {
        buf[i] = c;
    }

    bhmsg = block_header__unpack (NULL, i, buf);
    free(buf);
    if (bhmsg == NULL) {
        fprintf(stderr, "Error unpacking BlockHeader message\n");
        return 1;
    }

    length = bhmsg->datasize;
    if (verbose) fprintf(stderr, "Type: %s\nLength: %u\n", bhmsg->type, length);
    if (length <= 0 || length > MAX_BLOB_SIZE) {
        fprintf(stderr, "Blob isn't present or exceeds minimum/maximum size\n");
        return 1;
    }

    if (strcmp(bhmsg->type, "OSMHeader") == 0) {
        state = osmheader;
    } else if (strcmp(bhmsg->type, "OSMData") == 0) {
        state = osmdata;
    }

    block_header__free_unpacked (bhmsg, &protobuf_c_system_allocator);

    /* We are now reading the 'Blob' */
    buf = (unsigned char *) malloc(length * sizeof(unsigned char *));
    for (i = 0; i < length && (c=fgetc(stdin)) != EOF; i++) {
        buf[i] = c;
    }

    bmsg = blob__unpack (NULL, i, buf);
    if (bmsg == NULL) {
        fprintf(stderr, "Error unpacking Blob message\n");
        return 1;
    }
    free(buf);
    
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

        if (verbose) fprintf(stderr, "%s\n", hmsg->required_features[0]);

        header_block__free_unpacked (hmsg, &protobuf_c_system_allocator);
    } else if (state == osmdata) {
        /*
         * Unpack Block and check if all went well
         */
        PrimitiveBlock *pmsg = primitive_block__unpack (NULL, bmsg->raw_size, uncompressed);
        if (pmsg == NULL) {
            fprintf(stderr, "Error unpacking PrimitiveBlock message\n");
            return 1;
        }

        double lat_offset = NANO_DEGREE * pmsg->lat_offset;
        double lon_offset = NANO_DEGREE * pmsg->lon_offset;
        double granularity = NANO_DEGREE * pmsg->granularity;

        if (verbose) fprintf(stderr, "\t""Granularity: %d""\n", pmsg->granularity);
        if (verbose) fprintf(stderr, "\t""Primitive groups: %li""\n", pmsg->n_primitivegroup);
        for (unsigned int j = 0; j < pmsg->n_primitivegroup; j++) {
            if (verbose) fprintf(stderr,"\t\t""Nodes: %li""\n"\
                   "\t\t""Ways: %li""\n"\
                   "\t\t""Relations: %li""\n",
                   (pmsg->primitivegroup[j]->dense ?
                    pmsg->primitivegroup[j]->dense->n_id :
                           pmsg->primitivegroup[j]->n_nodes),
                   pmsg->primitivegroup[j]->n_ways,
                   pmsg->primitivegroup[j]->n_relations);

            /* TODO: Nodes is *untested* */
            if (pmsg->primitivegroup[j]->n_nodes > 0) {
                for (int k = 0; k < pmsg->primitivegroup[j]->n_nodes; k++) {
                    Node *node = pmsg->primitivegroup[j]->nodes[k];

                    printf("\t""<node id=\"%li\" lat=\"%.07f\" lon=\"%.07f\"",
                        node->id,
                        lat_offset + (node->lat * granularity),
                        lon_offset + (node->lon * granularity));
                    if (node->info) {
                        Info *info = node->info;
                        if (info->has_version) {
                            printnumericattribute("version", info->version);
                        }
                        if (info->has_changeset) {
                            printnumericattribute("changeset", info->changeset);
                        }
                        if (info->has_user_sid) {
                            ProtobufCBinaryData user = pmsg->stringtable->s[info->user_sid];
                            printuser(user);
                        }
                        if (info->has_uid) {
                            printnumericattribute("uid", info->uid);
                        }
                        if (info->has_timestamp) {
                            printtimestamp("timestamp", info->timestamp);
                        }
                    }

                    if (node->n_keys == 0 || node->n_vals == 0) {
                        fputs_unlocked("/>""\n", stdout);
                    } else {
                        fputs_unlocked(">""\n", stdout);

                        for (int l = 0; l < node->n_keys; l++) {
                            ProtobufCBinaryData key = pmsg->stringtable->s[node->keys[l]];
                            ProtobufCBinaryData val = pmsg->stringtable->s[node->vals[l]];

                            printtag(key, val);
                        }

                        fputs_unlocked("\t""</node>""\n", stdout);
                    }
                }
            }
            else
            if (pmsg->primitivegroup[j]->n_ways > 0) {
                for (int k = 0; k < pmsg->primitivegroup[j]->n_ways; k++) {
                    Way *way = pmsg->primitivegroup[j]->ways[k];
                    printsotid("way", way->id);
                    if (way->info) {
                        Info *info = way->info;
                        if (info->has_version) {
                            printnumericattribute("version", info->version);
                        }
                        if (info->has_changeset) {
                            printnumericattribute("changeset", info->changeset);
                        }
                        if (info->has_user_sid) {
                            ProtobufCBinaryData user = pmsg->stringtable->s[info->user_sid];
                            printuser(user);
                        }
                        if (info->has_uid) {
                            printnumericattribute("uid", info->uid);
                        }
                        if (info->has_timestamp) {
                            printtimestamp("timestamp", info->timestamp);
                        }
                    }

                    if ((way->n_keys == 0 || way->n_vals == 0) && way->n_refs == 0) {
                        fputs_unlocked("/>""\n", stdout);
                    } else {
                        long int deltaref = 0;
                        
                        fputs_unlocked(">""\n", stdout);
                        
                        for (int l = 0; l < way->n_refs; l++) {
                            deltaref += way->refs[l];
                            printnd(deltaref);
                        }

                        for (int l = 0; l < way->n_keys; l++) {
                            ProtobufCBinaryData key = pmsg->stringtable->s[way->keys[l]];
                            ProtobufCBinaryData val = pmsg->stringtable->s[way->vals[l]];

                            printtag(key, val);
                        }

                        fputs_unlocked("\t""</way>""\n", stdout);
                    }
                }
            }
            else
            if (pmsg->primitivegroup[j]->n_relations > 0) {
                for (int k = 0; k < pmsg->primitivegroup[j]->n_relations; k++) {
                    Relation *relation = pmsg->primitivegroup[j]->relations[k];
                    printsotid("relation", relation->id);
                    if (relation->info) {
                        Info *info = relation->info;
                        if (info->has_version) {
                            printnumericattribute("version", info->version);
                        }
                        if (info->has_changeset) {
                            printnumericattribute("changeset", info->changeset);
                        }
                        if (info->has_user_sid) {
                            ProtobufCBinaryData user = pmsg->stringtable->s[info->user_sid];
                            printuser(user);
                        }
                        if (info->has_uid) {
                            printnumericattribute("uid", info->uid);
                        }
                        if (info->has_timestamp) {
                            printtimestamp("timestamp", info->timestamp);
                        }
                    }

                    if ((relation->n_keys == 0 || relation->n_vals == 0) && relation->n_memids == 0) {
                        fputs_unlocked("/>""\n", stdout);
                    } else {
                        long int deltamemids = 0;
                        
                        fputs_unlocked(">""\n", stdout);
                        
                        for (int l = 0; l < relation->n_memids; l++) {
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

                            printmember(type, deltamemids, role);
                        }

                        for (int l = 0; l < relation->n_keys; l++) {
                            ProtobufCBinaryData key = pmsg->stringtable->s[relation->keys[l]];
                            ProtobufCBinaryData val = pmsg->stringtable->s[relation->vals[l]];

                            printtag(key, val);
                        }

                        fputs_unlocked("\t""</relation>""\n", stdout);
                    }
                }
            }
            else
            if (pmsg->primitivegroup[j]->n_changesets > 0) {
                for (int k = 0; k < pmsg->primitivegroup[j]->n_changesets; k++) {
                    ChangeSet *changeset = pmsg->primitivegroup[j]->changesets[k];

                    printsotid("changeset", changeset->id);
                    if (changeset->info) {
                        Info *info = changeset->info;
                        /* Not in Changeset                    
                        if (info->has_version) {
                            printnumericattribute("version", info->version);
                        }
                        if (info->has_changeset) {
                            printnumericattribute("changeset", info->changeset);
                        }*/
                        if (info->has_user_sid) {
                            ProtobufCBinaryData user = pmsg->stringtable->s[info->user_sid];
                            printuser(user);
                        }
                        if (info->has_uid) {
                            printnumericattribute("uid", info->uid);
                        }
                        if (info->has_timestamp) {
                            printtimestamp("created_at", info->timestamp);

                            if (changeset->has_closetime_delta) {
                                printtimestamp("closed_at", (info->timestamp + changeset->closetime_delta));
                            }
                        }
                    }

                    printf(" open=\"%s\" min_lon=\"%.07f\" min_lat=\"%.07f\" max_lon=\"%.07f\" max_lat=\"%.07f\"",
                        (changeset->open ? "true" : "false"),
                        lat_offset + (changeset->bbox->left * granularity),
                        lon_offset + (changeset->bbox->bottom * granularity),
                        lat_offset + (changeset->bbox->right * granularity),
                        lon_offset + (changeset->bbox->top * granularity));

                    if (changeset->n_keys == 0 || changeset->n_vals == 0) {
                        fputs_unlocked("/>""\n", stdout);
                    } else {
                        fputs_unlocked(">""\n", stdout);

                        for (int l = 0; l < changeset->n_keys; l++) {
                            ProtobufCBinaryData key = pmsg->stringtable->s[changeset->keys[l]];
                            ProtobufCBinaryData val = pmsg->stringtable->s[changeset->vals[l]];

                            printtag(key, val);
                        }

                        fputs_unlocked("\t""</changeset>""\n", stdout);
                    }
                }
            }
            else
            if (pmsg->primitivegroup[j]->dense) {
                int l = 0;
                unsigned long int deltaid = 0;
                long int deltalat = 0;
                long int deltalon = 0;
                unsigned long int deltatimestamp = 0;
                unsigned long int deltachangeset = 0;
                long int deltauid = 0;
                unsigned long int deltauser_sid = 0;

                DenseNodes *dense = pmsg->primitivegroup[j]->dense;
                

                for (int k = 0; k < dense->n_id; k++) {
                    unsigned char has_tags = 0;
                    deltaid += dense->id[k];
                    deltalat += dense->lat[k];
                    deltalon += dense->lon[k];
                    
                    printf("\t""<node id=\"%li\" lat=\"%.07f\" lon=\"%.07f\"", deltaid,
                            lat_offset + (deltalat * granularity),
                            lon_offset + (deltalon * granularity));

                    if (dense->denseinfo) {
                        DenseInfo *denseinfo = dense->denseinfo;

                        deltatimestamp += denseinfo->timestamp[k];
                        deltachangeset += denseinfo->changeset[k];
                        deltauid += denseinfo->uid[k];
                        deltauser_sid += denseinfo->user_sid[k];

                        printnumericattribute("version", denseinfo->version[k]);
                        printnumericattribute("changeset", deltachangeset);
			if (deltauid != -1) { // osmosis devs failed to read the specs
	                        printuser(pmsg->stringtable->s[deltauser_sid]);
	                        printnumericattribute("uid", deltauid);
			}
                        printtimestamp("timestamp", deltatimestamp);
                    }

                    if (l < dense->n_keys_vals) {
                        while (dense->keys_vals[l] != 0 && l < dense->n_keys_vals) {
                            if (has_tags < 1) {
                                has_tags++;
                                fputc_unlocked('>', stdout);
                            }
                            
                            ProtobufCBinaryData key = pmsg->stringtable->s[dense->keys_vals[l]];
                            ProtobufCBinaryData val = pmsg->stringtable->s[dense->keys_vals[l+1]];

                            printtag(key, val);

                            l += 2;
                        }
                        l += 1;
                    }

                    if (has_tags < 1) {
                        fputs_unlocked("/>""\n", stdout);
                    } else {
                        fputs_unlocked("\t""</node>""\n", stdout);
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
/* vim: set ts=4 sw=4 tw=79 expandtab :*/
