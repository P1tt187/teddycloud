#ifndef _CACHE_H
#define _CACHE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Structure representing a cache entry.
 */
typedef struct cache_entry_s cache_entry_t;
struct cache_entry_s
{
    cache_entry_t *next;      /**< Pointer to the next cache entry in the list. */
    uint32_t hash;            /**< Uppermost 32 bits of the hash for fast searching and inserting. */
    uint32_t statusCode;      /**< Status code when fetching the file. */
    bool exists;              /**< Flag indicating if the local cached file exists. */
    const char *original_url; /**< URL from which the file is to be downloaded. */
    const char *cached_url;   /**< URL generated and used when adding to index. */
    const char *file_path;    /**< Path of the local cached file. */
};

/**
 * @brief Adds a new cache entry for the given URL.
 *
 * @param url The URL to add to the cache.
 * @return Pointer to the newly created cache entry, or NULL if the addition fails.
 */
cache_entry_t *cache_add(const char *url);

/**
 * @brief Fetches the file for the given cache entry.
 *
 * @param entry Pointer to the cache entry.
 * @return true if the file was successfully fetched and exists locally, false otherwise.
 */
bool cache_fetch_entry(cache_entry_t *entry);

/**
 * @brief Searches for a cache entry by the original URL.
 *
 * @param url The original URL to search for in the cache.
 * @return Pointer to the corresponding cache entry if found, or NULL if not found.
 */
cache_entry_t *cache_fetch_by_url(const char *url);

/**
 * @brief Searches for a cache entry by the path used in the HTTP request.
 *
 * @param path The URI path to search for in the cache (e.g., '/cache/[hash].[ext]').
 * @return Pointer to the corresponding cache entry if found, or NULL if not found.
 */
cache_entry_t *cache_fetch_by_path(const char *path);

/**
 * @brief Searches for a cache entry by the cached URL.
 *
 * @param cached_url The cached URL to search for in the cache.
 * @return Pointer to the corresponding cache entry if found, or NULL if not found.
 */
cache_entry_t *cache_fetch_by_cached_url(const char *cached_url);

#endif // _CACHE_H