#ifndef PARSE_H
#define PARSE_H


#include "hashmap.h"
#include "String.h"
#include "str_setup.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


// Helper function to clean and normalize a word - removes numbers and handles contractions
char* cleanword(const char* token, char* output, u32 output_size) {
    if (!token || !output || output_size == 0) { return NULL; }
    
    u64 len = strlen(token);
    u32 out_idx = 0;
    bool has_letters = false;
    
    for (u64 i = 0; i < len && out_idx < output_size - 1; i++) {
        unsigned char c = (unsigned char)token[i];
        
        // Skip digits completely - remove numbers like 1023
        if (isdigit(c)) {
            continue;
        }
        
        // Handle apostrophes (both ASCII ' and UTF-8 curly quotes)
        // UTF-8 right single quote is E2 80 99
        if (c == '\'' || (c == 0xE2 && i + 2 < len && 
                          (unsigned char)token[i+1] == 0x80 && 
                          (unsigned char)token[i+2] == 0x99)) {
            
            // If it's a UTF-8 curly quote, skip the 3-byte sequence
            u64 next_pos = i + 1;
            if (c == 0xE2) {
                i += 2; // Skip the remaining bytes of UTF-8 sequence
                next_pos = i + 1;
            }
            
            // Check what comes after the apostrophe
            bool has_letter_after = false;
            bool is_possessive = false;
            
            for (u32 j = next_pos; j < len; j++) {
                unsigned char next = (unsigned char)token[j];
                if (isalpha(next)) {
                    has_letter_after = true;
                    // Check if it's possessive ('s or 's at end of word)
                    if (tolower(next) == 's') {
                        // Look ahead to see if there's anything after the 's'
                        bool has_more = false;
                        for (u32 k = j + 1; k < len; k++) {
                            if (isalpha((unsigned char)token[k])) {
                                has_more = true;
                                break;
                            }
                        }
                        if (!has_more) {
                            is_possessive = true;
                        }
                    }
                    break;
                }
                // Skip over more apostrophes/quotes
                if (next != '\'' && next != 0xE2) {
                    break;
                }
            }
            
            // Keep apostrophe if we have letters before AND after, but NOT if it's possessive
            if (has_letters && has_letter_after && !is_possessive) {
                output[out_idx++] = '\'';
            }
            continue;
        }
        
        // Convert to lowercase for case insensitivity
        if (isalpha(c)) {
            c = tolower(c);
            output[out_idx++] = c;
            has_letters = true;
        }
    }
    
    output[out_idx] = '\0';
    
    // Only return valid words that contain at least one letter
    return (out_idx > 0 && has_letters) ? output : NULL;
}


int parse(void)
{
    hashmap* map = hashmap_create(sizeof(String), sizeof(int), 
                                 murmurhash3_string, string_custom_delete, 
                                 NULL, string_custom_compare);

    // Basic delimiters - we'll handle punctuation more carefully
    const char* delim = " \n\t\r";
    
    FILE* f = fopen("../shakespeare.txt", "re");
    if (!f) {
        printf("error opening file\n");
        hashmap_destroy(map);
        return -1;
    }

    char line[512];
    char cleaned[256];
    u32 totalwords = 0;
    
    while (fgets(line, sizeof(line), f)) {
        char* token = strtok(line, delim);
        while (token) {
            if (strlen(token) > 0) {
                // Clean and normalize the word
                if (cleanword(token, cleaned, sizeof(cleaned))) 
                {
                    printf("%s\t", cleaned);
                    String str;
                    string_create_onstk(&str, cleaned);
                    int count;
                    if (hashmap_get(map, (u8*)&str, (u8*)&count) == 0) {
                        count++;
                    }
                    else {
                        count = 1;
                    }
                    hashmap_put(map, (u8*)&str, (u8*)&count);
                    //hashmap now owns the string buffer so don't delete
                                        
                    totalwords++;
                }
            }
            token = strtok(NULL, delim);
        }
    }
    

    // Print summary
    printf("\nTotal words processed: %lu\n", totalwords);
    printf("Unique words: %lu\n\n", map->size);

    String str;
    string_create_onstk(&str, "gay");
    int count;
    if (hashmap_get(map, (u8*)&str, (u8*)&count) == 0) {
        printf("Count of %s : %d", string_to_cstr(&str), count);
    }
    else {
        printf("not found\n");
    }

    string_destroy_fromstk(&str);
    hashmap_destroy(map);    
    printf("\n");
    return fclose(f);
}


#endif // PARSE_H
