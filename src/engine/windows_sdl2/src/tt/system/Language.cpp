#if defined(_XBOX)
#include <xtl.h>
#else
#include <windows.h>
#endif

#include <tt/loc/LocStr.h>
#include <tt/platform/tt_error.h>
#include <tt/system/Language.h>


namespace tt {
namespace system {

void Language::preInit()
{
}


std::string Language::setLocStrLanguage(loc::LocStr* p_locStr)
{
	// Dummy implementation for windows. (Only osx has a preferred lang. array.)
	std::string returnStr(getLanguage());
	if (p_locStr->supportsLanguage(returnStr) == false)
	{
		TT_WARN("LocStr object does not support language '%s'. Defaulting to 'en'.", returnStr.c_str());
		returnStr = "en";
		TT_ASSERTMSG(p_locStr->supportsLanguage(returnStr),
		             "LocStr object does not support default language '%s'.",
		             returnStr.c_str());
	}
	
	p_locStr->selectLanguage(returnStr);
	return returnStr;
}


std::string Language::getLanguage()
{
#if defined(_XBOX)
	//----------------------------------------------------------------------------------------------
	// Xbox 360 implementation
	
	const DWORD localeID = XGetLocale();
	const DWORD langID   = XGetLanguage();
	
	// Check if the user has set a locale that allows overriding of the language setting
	// NOTE: This might lead to undesired fixed localization based on location (as in Assassin's Creed, for example)
	switch (localeID)
	{
	case XC_LOCALE_NETHERLANDS: return "nl";
	case XC_LOCALE_BELGIUM:
		// According to Xbox 360 SDK documentation:
		// "The locale of XC_LOCAL_BELGIUM can be used to support Flemish,
		// but only if XGetLanguage returns the value XC_LANGUAGE_FRENCH"
		if (langID == XC_LANGUAGE_FRENCH) return "nl";
		break;
		
	default:
		// No overrides for this locale
		break;
	}
	
	// Simply use the user's language setting, without locale override
	switch (langID)
	{
	case XC_LANGUAGE_ENGLISH:  return "en";
	case XC_LANGUAGE_JAPANESE: return "jp";
	case XC_LANGUAGE_GERMAN:   return "de";
	case XC_LANGUAGE_FRENCH:   return "fr";
	case XC_LANGUAGE_SPANISH:  return "es";
	case XC_LANGUAGE_ITALIAN:  return "it";
	case XC_LANGUAGE_RUSSIAN:  return "ru";
		
	default:
		TT_PANIC("Unsupported Xbox Dashboard language: 0x%08X (defaulting to English)", langID);
		return "en";
	}
	
#else
	//----------------------------------------------------------------------------------------------
	// Windows implementation
	
#if defined(TT_BUILD_FINAL)
	const LANGID languageID = GetUserDefaultLangID();
	// LANGID http://msdn.microsoft.com/en-us/library/dd318693%28VS.85%29.aspx
	switch (PRIMARYLANGID(languageID))
	{
	case LANG_DUTCH:               return "nl";
	case LANG_ENGLISH:             return "en";
	case LANG_FRENCH:              return "fr";
	case LANG_GERMAN:              return "de";
	case LANG_ITALIAN:             return "it";
	case LANG_RUSSIAN:             return "ru";
	case LANG_SPANISH:             return "es";
	case LANG_PORTUGUESE:          return "pt"; //(SUBLANGID(languageID) == SUBLANG_PORTUGUESE_BRAZILIAN) ? "bz" : "pt";
	case LANG_NORWEGIAN:           return "no";
	case LANG_TURKISH:             return "tr";
	case LANG_CZECH:               return "cs";
	case LANG_POLISH:              return "pl";
	case LANG_FINNISH:             return "fi";
	case LANG_BULGARIAN:           return "bg";
	case LANG_JAPANESE:            return "jp";
	case LANG_CHINESE_SIMPLIFIED:  return "zh";
	case LANG_CHINESE_TRADITIONAL: return "zh";
	}
	
	TT_PANIC("No language mapping found for primary LANGID (0x%x). (Full user default LangID: 0x%x).",
	         PRIMARYLANGID(languageID), languageID);
	
	return "en";
#else
	// Non-final builds (dev/test) should be easy to switch so,
	// use geo location instead of language (language forces a restart!)
	// GEOID: http://msdn.microsoft.com/en-us/library/dd374073%28v=VS.85%29.aspx
	const GEOID id = GetUserGeoID(GEOCLASS_NATION);
	switch (id)
	{
		case 0x54: return "fr"; // France
		case 0x5E: return "de"; // Germany
		case 0x76: return "it"; // Italy
		case 0xD9: return "es"; // Spain
		case 0xB0: return "nl"; // Netherlands
		case 0x15: return "nl"; // Belgium
		case 0xF2: return "en"; // UK
		case 0xF4: return "en"; // US
		case 0xB1: return "no"; // Norway
		case 0xCB: return "ru"; // Russia
		case 0xC1: return "pt"; // Portugal
		case 0x20: return "pt"; // Brazil
		case 0xEB: return "tr"; // Turkey
		case 0x4B: return "cs"; // Czech Republic
		case 0x8F: return "cs"; // Slovakia
		case 0xBF: return "pl"; // Poland
		case 0x4D: return "fi"; // Finland
		case 0x23: return "bg"; // Bulgaria
		case 0x7a: return "jp"; // Japan
		case 0x2D: return "zh"; // Chinese (Simplified)
	}
	
	TT_PANIC("No valid geographical location (0x%x) identifier has been found!",
	         id);
	
	return "en";
#endif
#endif  // !defined(_XBOX)
}


// Namespace end
}
}
