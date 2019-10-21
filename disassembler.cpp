#include <amd_comgr.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void printMetadataList(amd_comgr_metadata_node_t &list);
void printMetadataMap(amd_comgr_metadata_node_t &map);
void printMetadataString(amd_comgr_metadata_node_t &node);

char* g_fileName = NULL;

static int readFile(const char *path, char **buf) {
    *buf = NULL;
    size_t r = 0;
    long size = 0;

    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END)) goto fail;

    size = ftell(f);
    if (size <= 0) goto fail;
    if (fseek(f, 0, SEEK_SET)) goto fail;

    *buf = new char[size + 1];
    (*buf)[size] = '\0';

    r = fread(*buf, (size_t)1, (size_t)size, f);

    if (r != size) goto fail;

    fclose(f);

    return (int)size;
fail:
    fclose(f);
    if (buf) delete [] buf;
    return 0;
}

static int writeFile(const char *path, const char *buf, const size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(buf, 1, size, f);
    fflush(f);
    fclose(f);
    return w;
}

static int printToFile(const char *buf) {
    assert(g_fileName);
    FILE *f = fopen(g_fileName, "ab");
    if (!f) return 0;
    size_t w = fwrite(buf, 1, strlen(buf), f);
    fflush(f);
    fclose(f);
    return w;
}

void checkError(amd_comgr_status_t s, const char *log, ...) {
    if (s != AMD_COMGR_STATUS_SUCCESS) {
        va_list args;
        va_start(args, log);
        vprintf(log, args);
        va_end(args);
        printf("\n");
        exit(1);
    }
}

void printMetadataString(amd_comgr_metadata_node_t &node) {
    size_t size = 0;
    amd_comgr_status_t status;

    status = amd_comgr_get_metadata_string(node, &size, NULL);
    checkError(status, "Error(%d): get metadata string size", status);

    char* buf = new char[size + 1];
    status = amd_comgr_get_metadata_string(node, &size, buf);

    printToFile(buf);
    //printf("\n");

    delete [] buf;
}

void printMetadataList(amd_comgr_metadata_node_t &list) {
    size_t size = 0;
    amd_comgr_status_t status;
    amd_comgr_metadata_kind_t kind;

    status = amd_comgr_get_metadata_list_size(list, &size);
    checkError(status, "Error(%d): get metadata list", status);

    int count = 0;

    // print all metadata info
    for (size_t i = 0; i < size && status == AMD_COMGR_STATUS_SUCCESS; i++) {
        amd_comgr_metadata_node_t node;
        kind = AMD_COMGR_METADATA_KIND_NULL;

        status = amd_comgr_index_list_metadata(list, i, &node);
        checkError(status, "Error(%d): get kernel metadata index \"%d\"", status, i);

        status = amd_comgr_get_metadata_kind(node, &kind);
        checkError(status, "Error(%d): get kernel metadata(%d) kind", status, i);

        switch(kind) {
            case AMD_COMGR_METADATA_KIND_STRING:
                if (!count) {
                    printToFile("[ ");
                }
                printMetadataString(node);
                printToFile(" ");
                count++;
                break;
            case AMD_COMGR_METADATA_KIND_LIST:
                printMetadataList(node);
                break;
            case AMD_COMGR_METADATA_KIND_MAP:
                printMetadataMap(node);
                break;
            default:
                printf("Error: invalid metadata kind %d\n", kind);
                exit(1);
        }
    }
    if (count) {
        printToFile("]");
    }
}

static amd_comgr_status_t mapCallback(amd_comgr_metadata_node_t name, amd_comgr_metadata_node_t value, void *unused) {
    static bool first_time = true;
    static int level = 0;
    if (first_time) {
        first_time = false;
    } else {
        printToFile("\n");
    }
    for (size_t i = 0; i < level; i++) {
        printToFile("\t");
    }
    printMetadataString(name);
    printToFile(" : ");

    amd_comgr_status_t status;
    amd_comgr_metadata_kind_t kind = AMD_COMGR_METADATA_KIND_NULL;
    status = amd_comgr_get_metadata_kind(value, &kind);
    checkError(status, "Error(%d): get map value kind", status);

    switch(kind) {
        case AMD_COMGR_METADATA_KIND_STRING:
            printMetadataString(value);
            break;
        case AMD_COMGR_METADATA_KIND_LIST:
            level = 1;
            printMetadataList(value);
            level = 0;
            break;
        case AMD_COMGR_METADATA_KIND_MAP:
            level++;
            printMetadataMap(value);
            break;
        default:
            printf("Error: invalid map value kind %d\n", kind);
            exit(1);
    }

    return AMD_COMGR_STATUS_SUCCESS;
}

void printMetadataMap(amd_comgr_metadata_node_t &map) {
    amd_comgr_iterate_map_metadata(map, mapCallback, NULL);
}

void disassemble(const char* elffilename) {
    // 1. load the elf
    char* buf = NULL;
    int size = readFile(elffilename, &buf);
    if (!buf) {
        fprintf(stderr, "Fail to read file\n");
        return;
    }

    amd_comgr_status_t status;

    // 2. create the action
    amd_comgr_action_info_t action;
    status = amd_comgr_create_action_info(&action);
    checkError(status, "Error(%d): create action", status);
    status = amd_comgr_action_info_set_language(action, AMD_COMGR_LANGUAGE_HC);
    checkError(status, "Error(%d): set language", status);
    status = amd_comgr_action_info_set_isa_name(action, "amdgcn-amd-amdhsa--gfx900");
    checkError(status, "Error(%d): set isa name", status);
    //status = amd_comgr_action_set_logging(action, true);
    //checkError(status, "Error(%d): set logging", status);

    // 3. Load the elf into data set
    amd_comgr_data_set_t elfbinary;
    status = amd_comgr_create_data_set(&elfbinary);
    checkError(status, "Error(%d): create data set", (int)status);

    amd_comgr_data_t data;
    status = amd_comgr_create_data(AMD_COMGR_DATA_KIND_EXECUTABLE, &data);
    checkError(status, "Error(%d): create data", status);
    status = amd_comgr_set_data(data, size, buf);
    checkError(status, "Error(%d): set data", status);
    status = amd_comgr_set_data_name(data, "hipkernel");
    checkError(status, "Error(%d): set data name", status);
    status = amd_comgr_data_set_add(elfbinary, data);
    checkError(status, "Error(%d): data set add", status);

    free(buf);

    // 4. Disassembly the elfbinary
    amd_comgr_data_set_t disassembly;
    status = amd_comgr_create_data_set(&disassembly);
    checkError(status, "Error(%d): create data set", status);
    //AMD_COMGR_ACTION_DISASSEMBLE_EXECUTABLE_TO_SOURCE 
    //AMD_COMGR_ACTION_DISASSEMBLE_BYTES_TO_SOURCE 
    status = amd_comgr_do_action(AMD_COMGR_ACTION_DISASSEMBLE_EXECUTABLE_TO_SOURCE , action, elfbinary, disassembly);
    checkError(status, "Error(%d): do action", status);


    // Write out the disassembly
    amd_comgr_data_t disassembly_data;
    status = amd_comgr_create_data(AMD_COMGR_DATA_KIND_SOURCE, &disassembly_data);
    checkError(status, "Error(%d): create disassembly data ", status);
    status = amd_comgr_action_data_get_data(disassembly, AMD_COMGR_DATA_KIND_SOURCE, 0, &disassembly_data);
    checkError(status, "Error(%d): action get disassembly data ", status);
    size_t bytes = 0;
    status = amd_comgr_get_data(disassembly_data, &bytes, NULL);
    checkError(status, "Error(%d): get disassembly data size", status);
    buf = new char[bytes + 1];
    status = amd_comgr_get_data(disassembly_data, &bytes, buf);
    char filenamebuf[strlen(elffilename) + 15] = {0};
    strcat(filenamebuf, elffilename);
    strcat(filenamebuf, ".disassembly");
    writeFile(filenamebuf, buf, bytes);

    delete [] buf;

    g_fileName = filenamebuf;

    amd_comgr_metadata_node_t metadata;
    status = amd_comgr_get_data_metadata(data, &metadata);
    checkError(status, "Error(%d): get shader metadata", status);

    printToFile("\n============= Metadata =================\n");
    printMetadataMap(metadata);
    printToFile("\n");

    fprintf(stdout, "Disassembly file saved to %s\n", filenamebuf);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stdout, "Usage: disassembler <.hsaco file name>\n");
        fprintf(stdout, "<.hsaco file> can be generated by specify following env var before executing HIP/OpenCL app:\n");
        fprintf(stdout, "export LOADER_OPTIONS_APPEND=\"-dump-code=1 -dump-dir=<path/to/save/dump>\"\n");
    }
    disassemble(argv[1]);

    return 0;
}

