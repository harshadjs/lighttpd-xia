#ifndef DISABLE_XIA
#include <dirent.h>
#include <stdio.h>
#include "server.h"
#include "xia.h"
#include "network.h"
#include "xcache.h"

#include <tidy.h>
#include <buffio.h>
#include <fcntl.h>
#include <unistd.h>

struct xia_args {
	ChunkContext *ctx;
	char *root;
};

/* curl write callback, to fill tidy's input buffer...  */ 
uint write_cb(char *in, uint size, uint nmemb, TidyBuffer *out)
{
	uint r;
	r = size * nmemb;
	tidyBufAppend( out, in, r );
	return(r);
}

void publish_cid(char *root, ChunkContext *ctx, const char *attr_val, const char *dag)
{
	int i;
	long size = (long)(strchr(attr_val, ';') - attr_val);
	char filename[256];
	ChunkInfo *chunks;
	struct stat stat_buf;

	strcpy(filename, root);
	strcat(filename, "/");
	strncat(filename, attr_val, size);

	fprintf(stderr, "filename = %s\n", filename);
	if(stat(filename, &stat_buf) < 0) {
		perror("stat");
		return;
	}
	XputFile(ctx, filename, stat_buf.st_size, &chunks);
}

static void print_indent(int indent)
{
	while(indent--)
		fprintf(stderr, " ");
}

/* Traverse the document tree */ 
void publishCIDs(int indent, char *root, ChunkContext *ctx, TidyDoc doc, TidyNode tnod)
{
	TidyNode child;
	for (child = tidyGetChild(tnod); child; child = tidyGetNext(child))
	{
		ctmbstr name = tidyNodeGetName(child);
		if(name) {
			/* if it has a name, then it's an HTML tag ... */ 
			TidyAttr attr;
			/* walk the attribute list */ 
			print_indent(indent);
			fprintf(stderr, "<%s", name);
			for (attr = tidyAttrFirst(child); attr; attr = tidyAttrNext(attr)) {
				fprintf(stderr, " ");
				fprintf(stderr, tidyAttrName(attr));
				if(strcasecmp(tidyAttrName(attr), "src") == 0) {
					tidyAttrValue(attr) ? fprintf(stderr, "=\"%s", tidyAttrValue(attr)) : fprintf(stderr, " ");
				} else {
					tidyAttrValue(attr) ? fprintf(stderr, "=\"%s\"", tidyAttrValue(attr)) : fprintf(stderr, " ");					
					
				}
			}
			fprintf(stderr, ">\n");
		} else {
			/* if it doesn't have a name, then it's probably text, cdata, etc... */ 
			TidyBuffer buf;
			tidyBufInit(&buf);
			tidyNodeGetText(doc, child, &buf);
			print_indent(indent);
			fprintf(stderr, "%s\n", buf.bp ? (char *)buf.bp : "");
			tidyBufFree(&buf);

		}
		publishCIDs(indent + 4, root, ctx, doc, child); /* recursive */ 
		if(name) {
			print_indent(indent);
			fprintf(stderr, "</%s>\n", name);
		}
	}
}
 
static void read_file_into_buf(TidyBuffer *docbuf, const char *filename)
{
	int ret, fd;
	char buf[512];

	fd = open(filename, O_RDONLY);

	while(1) {
		ret = read(fd, buf, 512);
		if(ret <= 0)
			break;
		write_cb(buf, ret, 1, docbuf);
	}
}
 
static int html_parse(const char *root, ChunkContext *ctx, const char *fname)
{
	TidyDoc tdoc;
	TidyBuffer docbuf = {0};
	TidyBuffer tidy_errbuf = {0};
	int err;
 
	tdoc = tidyCreate();
	tidyOptSetBool(tdoc, TidyForceOutput, yes); /* try harder */ 
	tidyOptSetInt(tdoc, TidyWrapLen, 4096);
	tidySetErrorBuffer( tdoc, &tidy_errbuf );
	tidyBufInit(&docbuf);
 
	read_file_into_buf(&docbuf, fname);

	err = tidyParseBuffer(tdoc, &docbuf); /* parse the input */ 
	if(err >= 0) {
		fprintf(stderr, "Parsing\n");
		err = tidyCleanAndRepair(tdoc); /* fix any problems */ 
		if(err >= 0) {
			fprintf(stderr, "Yes 1\n");
			err = tidyRunDiagnostics(tdoc); /* load tidy error buffer */ 
			if(err >= 0) {
				fprintf(stderr, "Yes 2\n");
				publishCIDs(0, root, ctx, tdoc, tidyGetRoot(tdoc)); /* walk the tree */ 
//				fprintf(stderr, "%s\n", tidy_errbuf.bp); /* show errors */ 
			}
		}
	} else {
		fprintf(stderr, "ERROR\n");
	}
 
	/* clean-up */ 
	tidyBufFree(&docbuf);
	tidyBufFree(&tidy_errbuf);
	tidyRelease(tdoc);

	return(err);
}

static void fn_publish_cid(const char *fname, void *arg)
{
	ChunkContext *ctx = ((struct xia_args *)arg)->ctx;
	char *root = ((struct xia_args *)arg)->root;

	if(strstr(fname, ".html")) {
		fprintf(stderr, "Parsing %s\n", fname);
		html_parse(root, ctx, fname);
	}
}

static void dir_walk(const char *root, void *arg, void (*fn)(const char *, void *))
{
	DIR *dir;
	struct dirent entry, *result;
	char path[256];

	if(!root)
		return;

	dir = opendir(root);
	if(!dir) {
		return;
	}

	strcpy(path, root);

	while((readdir_r(dir, &entry, &result) == 0) && (result != NULL)) {
		char path1[256];

		if(strcmp(entry.d_name, ".") == 0) {
			continue;
		}
		if(strcmp(entry.d_name, "..") == 0) {
			continue;
		}

		strcpy(path1, path);
		strcat(path1, "/");
		strcat(path1, entry.d_name);

		if(entry.d_type == DT_DIR) {
			dir_walk(path1, arg, fn);
		} else {
			fn(path1, arg);
		}
	}
}

int xia_publish(server *srv, specific_config *s)
{
	ChunkContext *ctx;
	struct xia_args args;

	Xinit();

	ctx = XallocCacheSlice(2000000, 1000, 0);
	if(!ctx)
		return -1;

	args.ctx = ctx;
	args.root = s->document_root->ptr;

	//FIXME: srv unused for now.
	fprintf(stderr, "Config dir = %s\n", s->document_root->ptr);
	dir_walk(s->document_root->ptr, &args, fn_publish_cid);

	return 0;
}


#endif
