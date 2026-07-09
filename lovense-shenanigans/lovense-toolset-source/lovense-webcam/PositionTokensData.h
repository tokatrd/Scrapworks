#pragma once


#include <string>
#include <memory>
#include <map>
#include <unordered_map>


class TokenRule {
public:
	std::string m_position_name;
	int m_token_value = 0;
	int m_duration = 0;
	int m_panvalue = 0;
	int m_titlvalue = 0;
	long m_zoomvalue = 0;
	int m_index = -1;
};


using TokenMap = std::map<std::string, std::shared_ptr<TokenRule>>;
using TokenVector = std::vector<std::pair<std::string, std::shared_ptr<TokenRule>>>;
using TokenVectorItem = std::pair<std::string, std::shared_ptr<TokenRule>>;

extern void GetAllTokenInfo(TokenMap& rules);
extern void GetAllTokenInfo(TokenVector& rules);

