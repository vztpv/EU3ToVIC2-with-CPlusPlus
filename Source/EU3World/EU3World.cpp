﻿/*Copyright (c) 2014 The Paradox Game Converters Project

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/

// 2018.10.28 SOUTH KOREA (vztpv@naver.com)

#include "EU3World.h"
#include <algorithm>
#include <fstream>
#include "../Log.h"
#include "../Configuration.h"
#include "../Mapper.h"
#include "EU3Province.h"
#include "EU3Country.h"
#include "EU3Diplomacy.h"
#include "EU3Localisation.h"
#include "EU3Religion.h"

#include "wiz/load_data_utility.h"
#include "wiz/load_data.h"


EU3World::EU3World(const wiz::load_data::UserType* obj)
{
	cachedWorldType = unknown;

	std::string key;	

	/* //??
	std::vector<Object*> dateObj = obj->getValue("date");
	if (dateObj.size() > 0)
	{
		date endDate(dateObj[0]->getLeaf());
	}
	*/
	provinces.clear();
	countries.clear();
	for (int i = 0; i < obj->GetUserTypeListSize(); ++i)
	{
		key = obj->GetUserTypeList(i)->GetName().ToString();

		// Is this a numeric value? If so, must be a province
		if (wiz::load_data::Utility::IsInteger(key)) 
		{
			EU3Province* province = new EU3Province(obj->GetUserTypeList(i));
			provinces.insert(std::make_pair(province->getNum(), province));
		}

		// Countries are three uppercase characters
		else if ((key.size() == 3) && 
					(key.c_str()[0] >= 'A') && (key.c_str()[0] <= 'Z') && 
					(key.c_str()[1] >= 'A') && (key.c_str()[1] <= 'Z') && 
					(key.c_str()[2] >= 'A') && (key.c_str()[2] <= 'Z')
				  )
		{
			if ((key == "---") || (key == "REB") || (key == "PIR") || (key == "NAT"))
			{
				continue;
			}
			else
			{
				EU3Country* country = new EU3Country(obj->GetUserTypeList(i));
				
				countries.insert(std::make_pair(country->getTag(), country));
			}
		}
	}

	// add province owner info to countries
	for (std::map<int, EU3Province*>::iterator i = provinces.begin(); i != provinces.end(); ++i)
	{
		std::map<std::string, EU3Country*>::iterator j = countries.find( i->second->getOwnerString() );
		if (j != countries.end())
		{
			j->second->addProvince(i->second);
			i->second->setOwner(j->second);
		}
	}

	// add province core info to countries
	for (std::map<int, EU3Province*>::iterator i = provinces.begin(); i != provinces.end(); ++i)
	{
		std::vector<EU3Country*> cores = i->second->getCores(countries);
		for (std::vector<EU3Country*>::iterator j = cores.begin(); j != cores.end(); ++j)
		{
			(*j)->addCore(i->second);
		}
	}

	std::vector<wiz::load_data::UserType*> diploObj = obj->GetUserTypeItem("diplomacy");
	if (diploObj.size() > 0)
	{
		diplomacy = new EU3Diplomacy(diploObj[0]);
	}
	else
	{
		diplomacy = new EU3Diplomacy;
	}

	std::vector<wiz::load_data::UserType*> tradeObj = obj->GetUserTypeItem("trade");
	if (tradeObj.size() > 0)
	{
		std::vector<wiz::load_data::UserType*> COTsObj = tradeObj[0]->GetUserTypeItem("cot");
		for (std::vector<wiz::load_data::UserType*>::iterator i = COTsObj.begin(); i != COTsObj.end(); ++i)
		{
			int location = (*i)->GetItem("location")[0].Get(0).ToInt();
			std::map<int, EU3Province*>::iterator j = provinces.find(location);
			if (j != provinces.end())
			{
				j->second->setCOT(true);
			}
		}
	}

	// calculate total province weights
	worldWeightSum = 0;
	std::vector<double> provEconVec;
	std::map<std::string, std::vector<double> > world_tag_weights;
	for (std::map<int, EU3Province*>::iterator i = provinces.begin(); i != provinces.end(); ++i)
	{
		i->second->determineProvinceWeight();
		// 0: Goods produced; 1 trade goods price; 2: trade value efficiency; 3: production effiency; 4: trade value; 5: production income
		// 6: base tax; 7: building tax income 8: building tax eff; 9: total tax income; 10: total_trade_value

		provEconVec = i->second->getProvProductionVec();
		worldWeightSum += i->second->getTotalWeight();

		std::vector<double> map_values;
		// Total Base Tax, Total Tax Income, Total Production, Total Buildings, Total Manpower, total province weight //
		map_values.push_back((2 * i->second->getBaseTax()));
		map_values.push_back(i->second->getProvTaxIncome());
		map_values.push_back(i->second->getProvProdIncome());
		map_values.push_back(i->second->getProvTotalBuildingWeight());
		map_values.push_back(i->second->getProvMPWeight());
		map_values.push_back(i->second->getTotalWeight());
		
		if (world_tag_weights.count(i->second->getOwnerString())) {
			std::vector<double> new_map_values;
			new_map_values = world_tag_weights[i->second->getOwnerString()];
			new_map_values[0] += map_values[0];
			new_map_values[1] += map_values[1];
			new_map_values[2] += map_values[2];
			new_map_values[3] += map_values[3];
			new_map_values[4] += map_values[4];
			new_map_values[5] += map_values[5];

			world_tag_weights[i->second->getOwnerString()] = new_map_values;
			
		}
		else {
			world_tag_weights.insert(std::pair<std::string, std::vector<double> >(i->second->getOwnerString(), std::move(map_values)));
		}
	}

	LOG(LogLevel::Info) << "Sum of all Province Weights: " << worldWeightSum;

	// Total Base Tax, Total Tax Income, Total Production, Total Buildings, Total Manpower, total province weight //
	LOG(LogLevel::Info) << "World Tag Map Size: " << world_tag_weights.size();
}


void EU3World::setEU3WorldProvinceMappings(const inverseProvinceMapping& inverseProvinceMap)
{
	for (std::map<int, EU3Province*>::iterator i = provinces.begin(); i != provinces.end(); ++i)
	{
		i->second->setNumDestV2Provs(inverseProvinceMap.find(i->first)->second.size());
	}
}


void EU3World::readCommonCountries(std::istream& in, const std::string& rootPath)
{
	// Add any info from common\countries
	const int maxLineLength = 10000;
	char line[maxLineLength];

	while (true)
	{
		in.getline(line, maxLineLength);
		if (in.eof())
		{
			return;
		}
		std::string countryLine = line;
		if (countryLine.size() >= 6 && countryLine[0] != '#')
		{
			// First three characters must be the tag.
			std::string tag = countryLine.substr(0, 3);
			std::map<std::string, EU3Country*>::iterator findIter = countries.find(tag);
			if (findIter != countries.end())
			{
				EU3Country* country = findIter->second;

				// The country file name is all the text after the equals sign (possibly in quotes).
				size_t equalPos = countryLine.find('=', 3);
				size_t beginPos = countryLine.find_first_not_of(' ', equalPos + 1);
				size_t endPos = countryLine.find_last_not_of(' ') + 1;
				std::string fileName = countryLine.substr(beginPos, endPos - beginPos);
				if (fileName.front() == '"' && fileName.back() == '"')
				{
					fileName = fileName.substr(1, fileName.size() - 2);
				}
				std::replace(fileName.begin(), fileName.end(), '/', '\\');

				// Parse the country file.
				std::string path = rootPath + "\\common\\" + fileName;
				size_t lastPathSeparatorPos = path.find_last_of('\\');
				std::string localFileName = path.substr(lastPathSeparatorPos + 1, std::string::npos);

				{
					wiz::load_data::UserType ut;
					wiz::load_data::LoadData::LoadDataFromFile3(path, ut, -1, 0);
					country->readFromCommonCountry(localFileName, &ut);
				}
			}
		}
	}
}


EU3Country* EU3World::getCountry(std::string tag) const
{
	std::map<std::string, EU3Country*>::const_iterator i = countries.find(tag);
	if (i != countries.end())
	{
		return i->second;
	}
	else
	{
		return NULL;
	}
}


EU3Province* EU3World::getProvince(int provNum) const
{
	std::map<int, EU3Province*>::const_iterator i = provinces.find(provNum);
	if (i != provinces.end())
	{
		return i->second;
	}
	else
	{
		return NULL;
	}
}


void EU3World::removeCountry(std::string tag)
{
	countries.erase(tag);
}


void EU3World::resolveRegimentTypes(const RegimentTypeMap& rtMap)
{
	for (std::map<std::string, EU3Country*>::iterator itr = countries.begin(); itr != countries.end(); ++itr)
	{
		itr->second->resolveRegimentTypes(rtMap);
	}
}


WorldType EU3World::getWorldType()
{
	if (cachedWorldType != unknown)
	{
		return cachedWorldType;
	}

	int maxProvinceID = 0;
	for (std::map<int, EU3Province*>::iterator itr = provinces.begin(); itr != provinces.end(); ++itr)
	{
		if ( itr->first > maxProvinceID )
		{
			maxProvinceID = itr->first;
		}
	}

	switch (maxProvinceID)
	{
	case 1774:
	case 1775:
		cachedWorldType = InNomine;
		break;
	case 1814:
		cachedWorldType = HeirToTheThrone;
		break;
	case 1882:
		cachedWorldType = DivineWind;
		break;
	default:
		Log(LogLevel::Warning) << "Unrecognized max province ID: " << maxProvinceID;
		if (maxProvinceID < 1774)
		{
			cachedWorldType = VeryOld; // pre-IN
		}
		else
		{
			cachedWorldType = unknown; // possibly modded
		}
		break;
	}

	// Allow the configuration file to override the game type
	std::string configWorldType = Configuration::getEU3Gametype();
	WorldType forcedWorldType = unknown;
	if (configWorldType == "dw")
	{
		forcedWorldType = DivineWind;
	}
	else if (configWorldType == "httt")
	{
		forcedWorldType = HeirToTheThrone;
	}
	else if (configWorldType == "in")
	{
		forcedWorldType = InNomine;
	}
	else if (configWorldType == "auto")
	{
		forcedWorldType = cachedWorldType;
	}

	if ((cachedWorldType != forcedWorldType) && (cachedWorldType != unknown))
	{
		Log(LogLevel::Warning) << "World type was detected successfuly, but a different type was specified in the configuration file!";
	}

	if (cachedWorldType == unknown)
	{
		Log(LogLevel::Warning) << "World type unknown!";
	}

	if (forcedWorldType != unknown)
	{
		cachedWorldType = forcedWorldType;
	}

	return cachedWorldType;
}


void EU3World::checkAllProvincesMapped(const inverseProvinceMapping& inverseProvinceMap) const
{
	for (std::map<int, EU3Province*>::const_iterator i = provinces.begin(); i != provinces.end(); ++i)
	{
		inverseProvinceMapping::const_iterator j = inverseProvinceMap.find(i->first);
		if (j == inverseProvinceMap.end())
		{
			LOG(LogLevel::Warning) << "No mapping for province " << i->first;
		}
	}
}


void EU3World::checkAllEU3CulturesMapped(const cultureMapping& cultureMap, const inverseUnionCulturesMap& inverseUnionCultures) const
{
	for (auto cultureItr = inverseUnionCultures.begin(); cultureItr != inverseUnionCultures.end(); ++cultureItr)
	{
		std::string	EU3Culture	= cultureItr->first;
		bool		matched		= false;
		for (auto mapItr = cultureMap.begin(); mapItr != cultureMap.end(); mapItr++)
		{
			if (mapItr->srcCulture == EU3Culture)
			{
				matched = true;
				break;
			}
		}
		if (!matched)
		{
			LOG(LogLevel::Warning) << "No culture mapping for EU3 culture " << EU3Culture;
		}
	}
}


void EU3World::checkAllEU3ReligionsMapped(const religionMapping& religionMap) const
{
	std::map<std::string, EU3Religion*> allReligions = EU3Religion::getAllReligions();
	for (auto religionItr = allReligions.begin(); religionItr != allReligions.end(); ++religionItr)
	{
		auto mapItr = religionMap.find(religionItr->first);
		if (mapItr == religionMap.end())
		{
			Log(LogLevel::Warning) << "No religion mapping for EU3 religion " << religionItr->first;
		}
	}
}


void EU3World::setLocalisations(const EU3Localisation& localisation)
{
	for (std::map<std::string, EU3Country*>::iterator countryItr = countries.begin(); countryItr != countries.end(); ++countryItr)
	{
		const auto& nameLocalisations = localisation.GetTextInEachLanguage(countryItr->first);
		for (const auto& nameLocalisation : nameLocalisations)
		{
			unsigned int			language = nameLocalisation.first;
			const std::string&	name = nameLocalisation.second;
			countryItr->second->setLocalisationName(language, name);
		}
		const auto& adjectiveLocalisations = localisation.GetTextInEachLanguage(countryItr->first + "_ADJ");
		for (const auto& adjectiveLocalisation : adjectiveLocalisations)
		{
			unsigned int			language = adjectiveLocalisation.first;
			const std::string&	adjective = adjectiveLocalisation.second;
			countryItr->second->setLocalisationAdjective(language, adjective);
		}
	}
}
