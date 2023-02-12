#include <algorithm>

#include "tcgplayer.h"

void curlTcgplayer(char* cardName, std::vector<CardListing>& listings, struct MemoryStruct& tcgplayerResponse, CURL* curl, CURLcode& res) {
	nlohmann::json jsonObj = R"(
		{
			"size": 24,
			"filters": {
				"term": {
					"productLineName": ["yugioh"],
					"productName": ["Blue-Eyes White Dragon"],
					"language": ["English"]
				}
			},
			"listingSearch": {
				"filters": {
					"term": {
						"sellerStatus":"Live",
						"channelId": 0
					},
					"range": {
						"quantity": {
							"gte": 1
						}
					}
				}
			},
			"context": {
				"shippingCountry": "US"
			},
			"sort": {
				"field": "market-price",
				"order": "asc"
			}
		}
	)"_json;
	jsonObj["filters"]["term"]["productName"] = { cardName };
	std::string jsonStr = jsonObj.dump();
    tcgplayerResponse.memory = (char*)malloc(1);
    tcgplayerResponse.size = 0;
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        nlohmann::json responseJson = nlohmann::json::parse(tcgplayerResponse.memory);
        for (const auto& result : responseJson["results"][0]["results"]) {
            listings.push_back(CardListing(result["customAttributes"]["number"].dump(), result["rarityName"].dump(), result["marketPrice"]));
        }
        if (listings.size() > 0) {
            while (listings[0].price == 0) {
                std::rotate(listings.begin(), listings.begin() + 1, listings.end());
            }
        }
    }
    free(tcgplayerResponse.memory);
}