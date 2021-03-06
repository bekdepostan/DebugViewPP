// (C) Copyright Gert-Jan de Vos and Jan Wilmans 2013.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at 
// http://www.boost.org/LICENSE_1_0.txt)

// Repository at: https://github.com/djeedjay/DebugViewPP/

#include "stdafx.h"
#include <boost/algorithm/string/case_conv.hpp>
#include "Win32/Registry.h"
#include "CobaltFusion/stringbuilder.h"
#include "DebugView++Lib/Colors.h"
#include "DebugView++Lib/Filter.h"

namespace fusion {
namespace debugviewpp {

std::string MatchKey(const std::smatch& match, MatchType::type matchType)
{
	if (matchType == MatchType::RegexGroups && match.size() > 1)
	{
		std::string key;
		auto it = match.begin();
		while (++it != match.end())
		{
			if (!key.empty())
				key.append(1, '\0');
			key += boost::to_lower_copy(it->str());
		}
		return key;
	}
	else
	{
		return boost::to_lower_copy(match.str());
	}
}

Filter::Filter() :
	matchType(MatchType::Simple),
	filterType(FilterType::Include),
	bgColor(RGB(255, 255, 255)),
	fgColor(RGB(  0,   0,   0)),
	enable(true),
	matched(false)
{
}

Filter::Filter(const std::string& text, MatchType::type matchType, FilterType::type filterType, COLORREF bgColor, COLORREF fgColor, bool enable, bool matched) :
	text(text), re(MakePattern(matchType, text), std::regex_constants::icase | std::regex_constants::optimize), matchType(matchType), filterType(filterType), bgColor(bgColor), fgColor(fgColor), enable(enable), matched(matched)
{
}

void SaveFilterSettings(const std::vector<Filter>& filters, CRegKey& reg)
{
	int i = 0;
	for (auto it = filters.begin(); it != filters.end(); ++it, ++i)
	{
		CRegKey regFilter;
		regFilter.Create(reg, WStr(wstringbuilder() << L"Filter" << i));
		regFilter.SetStringValue(L"", WStr(it->text));
		regFilter.SetDWORDValue(L"MatchType", MatchTypeToInt(it->matchType));
		regFilter.SetDWORDValue(L"FilterType", FilterTypeToInt(it->filterType));
		regFilter.SetDWORDValue(L"Type", FilterTypeToInt(it->filterType));
		regFilter.SetDWORDValue(L"BgColor", it->bgColor);
		regFilter.SetDWORDValue(L"FgColor", it->fgColor);
		regFilter.SetDWORDValue(L"Enable", it->enable);
	}
}

// Temporary backward compatibilty for loading FilterType::MatchColor:
Filter MakeFilter(const std::string& text, MatchType::type matchType, FilterType::type filterType, COLORREF bgColor, COLORREF fgColor, bool enable, bool matched)
{
	if (filterType == FilterType::MatchColor)
	{
		filterType = FilterType::Token;
		bgColor = Colors::Auto;
	}
	return Filter(text, matchType, filterType, bgColor, fgColor, enable, matched);
}

void LoadFilterSettings(std::vector<Filter>& filters, CRegKey& reg)
{
	for (int i = 0; ; ++i)
	{
		CRegKey regFilter;
		if (regFilter.Open(reg, WStr(wstringbuilder() << L"Filter" << i)) != ERROR_SUCCESS)
			break;

		filters.push_back(MakeFilter(
			Str(Win32::RegGetStringValue(regFilter)),
			IntToMatchType(Win32::RegGetDWORDValue(regFilter, L"MatchType", MatchType::Regex)),
			IntToFilterType(Win32::RegGetDWORDValue(regFilter, L"Type")),
			Win32::RegGetDWORDValue(regFilter, L"BgColor", Colors::BackGround),
			Win32::RegGetDWORDValue(regFilter, L"FgColor", Colors::Text),
			Win32::RegGetDWORDValue(regFilter, L"Enable", 1) != 0));
	}
}

bool IsIncluded(std::vector<Filter>& filters, const std::string& text, MatchColors& matchColors)
{
	for (auto it = filters.begin(); it != filters.end(); ++it)
	{
		if (!it->enable)
			continue;

		if (it->filterType == FilterType::Exclude && std::regex_search(text, it->re))
			return false;
	}

	bool included = false;
	bool includeFilterPresent = false;
	for (auto it = filters.begin(); it != filters.end(); ++it)
	{
		if (!it->enable)
			continue;

		if (it->bgColor == Colors::Auto)
		{
			std::sregex_iterator begin(text.begin(), text.end(), it->re), end;
			for (auto tok = begin; tok != end; ++tok)
			{
				auto key = MatchKey(*tok, it->matchType);
				if (matchColors.find(key) == matchColors.end())
					matchColors.emplace(std::make_pair(key, GetRandomBackColor()));
			}
		}

		if (it->filterType == FilterType::Include)
		{
			includeFilterPresent = true;
			included |= std::regex_search(text, it->re);
		}

		if (it->filterType == FilterType::Once && std::regex_search(text, it->re))
		{
			included |= !it->matched;
			it->matched = true;
		}
	}

	return !includeFilterPresent || included;
}

bool MatchFilterType(const std::vector<Filter>& filters, FilterType::type type, const std::string& text)
{
	for (auto it = filters.begin(); it != filters.end(); ++it)
	{
		if (it->enable && it->filterType == type && std::regex_search(text, it->re))
			return true;
	}

	return false;
}

} // namespace debugviewpp 
} // namespace fusion
