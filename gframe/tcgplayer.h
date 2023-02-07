#ifndef TCGPLAYER_H_
#define TCGPLAYER_H_

#include <string>

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

#endif