/*
 * File: cache.c
 *
 * Copyright 2000-2009 Jorge Arellano Cid <jcid@dillo.org>
 * Copyright 2025 Rodrigo Arias Mallo <rodarima@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __CACHE_H__
#define __CACHE_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "chain.h"
#include "url.h"

/*
 * Cache Op codes
 */
#define CA_Send    (0)  /* Normal update */
#define CA_Close   (1)  /* Successful operation close */
#define CA_Abort   (2)  /* Operation abort */

/*
 * Flag Defines
 */
#define CA_GotHeader       0x1  /* True after header is completely got */
#define CA_GotContentType  0x2  /* True after Content-Type is known */
#define CA_GotLength       0x4  /* True if Content-Length is known */
#define CA_InProgress      0x8  /* True if we are getting data */
#define CA_Redirect       0x10  /* Data actually points to a redirect */
#define CA_ForceRedirect  0x20  /* Unconditional redirect */
#define CA_TempRedirect   0x40  /* Temporary redirect */
#define CA_NotFound       0x80  /* True if remote server didn't find the URL */
#define CA_Aborted       0x100  /* Aborted before getting full data */
#define CA_MsgErased     0x200  /* Used to erase the bw's status bar */
#define CA_RedirectLoop  0x400  /* Redirect loop */
#define CA_InternalUrl   0x800  /* URL content is generated by dillo */
#define CA_HugeFile     0x1000  /* URL content is too big */
#define CA_IsEmpty      0x2000  /* True until a byte of content arrives */
#define CA_KeepAlive    0x4000

typedef struct CacheClient CacheClient_t;

/**
 * Callback type for cache clients
 */
typedef void (*CA_Callback_t)(int Op, CacheClient_t *Client);

/**
 * Data structure for cache clients.
 */
struct CacheClient {
   int Key;                 /**< Primary Key for this client */
   const DilloUrl *Url;     /**< Pointer to a cache entry Url */
   int Version;             /**< Dicache version of this Url (0 if not used) */
   void *Buf;               /**< Pointer to cache-data */
   uint_t BufSize;          /**< Valid size of cache-data */
   CA_Callback_t Callback;  /**< Client function */
   void *CbData;            /**< Client function data */
   void *Web;               /**< Pointer to the Web structure of our client */
};

/*
 * Function prototypes
 */
void a_Cache_init(void);
void a_Cache_entry_inject(const DilloUrl *Url, Dstr *data_ds);
int a_Cache_open_url(void *Web, CA_Callback_t Call, void *CbData);
int a_Cache_get_buf(const DilloUrl *Url, char **PBuf, int *BufSize);
void a_Cache_unref_buf(const DilloUrl *Url);
const char *a_Cache_get_content_type(const DilloUrl *url);
const char *a_Cache_set_content_type(const DilloUrl *url, const char *ctype,
                                     const char *from);
uint_t a_Cache_get_flags(const DilloUrl *url);
uint_t a_Cache_get_flags_with_redirection(const DilloUrl *url);
bool_t a_Cache_process_dbuf(int Op, const char *buf, size_t buf_size,
                          const DilloUrl *Url);
int a_Cache_download_enabled(const DilloUrl *url);
void a_Cache_entry_remove_by_url(DilloUrl *url);
void a_Cache_freeall(void);
CacheClient_t *a_Cache_client_get_if_unique(int Key);
void a_Cache_stop_client(int Key);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __CACHE_H__ */

