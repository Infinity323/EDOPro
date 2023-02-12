#ifndef TCGPLAYER_H_
#define TCGPLAYER_H_

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t write_callback(char* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    mem->memory = (char*)realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) {
    /* out of memory */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

struct CardListing {
    CardListing(std::string code, std::string rarity, float price) {
        this->code = code;
        if (code.length() <= 8) {
            this->code += "\t";
        }
        this->rarity = rarity;
        if (rarity.length() <= 24) {
            this->rarity += "\t";
        }
        else if (rarity.length() <= 16) {
            this->rarity += "\t\t";
        }
        else if (rarity.length() <= 8) {
            this->rarity += "\t\t\t";
        }
        this->price = price;
    }
    std::string code;
    std::string rarity;
    float price;
};

void curlTcgplayer(char* cardName, std::vector<CardListing>& listings, struct MemoryStruct& tcgplayerResponse, CURL* curl, CURLcode& res);

#endif