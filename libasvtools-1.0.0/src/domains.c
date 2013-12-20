#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "asvtools.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static struct
{
    int  n;
    char *abbrev, *title;
}
countries[] =
{
    {  0, "??", "Unknown"},
    {  1, "ad", "Andorra"},
    {  2, "ae", "United Arab Emirates"},
    {  3, "af", "Afghanistan"},
    {  4, "ag", "Antigua and Barbuda"},
    {  5, "ai", "Anguilla"},
    {  6, "al", "Albania"},
    {  7, "am", "Armenia"},
    {  8, "an", "Netherlands Antilles"},
    {  9, "ao", "Angola"},
    { 10, "aq", "Antarctica"},

    { 11, "ar", "Argentina"},
    { 12, "as", "American Samoa"},
    { 13, "at", "Austria"},
    { 14, "au", "Australia"},
    { 15, "aw", "Aruba"},
    { 16, "az", "Azerbaidjan"},
    { 17, "ba", "Bosnia-Herzegovina"},
    { 18, "bb", "Barbados"},
    { 19, "bd", "Bangladesh"},
    { 20, "be", "Belgium"},

    { 21, "bf", "Burkina Faso"},
    { 22, "bg", "Bulgaria"},
    { 23, "bh", "Bahrain"},
    { 24, "bi", "Burundi"},
    { 25, "bj", "Benin"},
    { 26, "bm", "Bermuda"},
    { 27, "bn", "Brunei Darussalam"},
    { 28, "bo", "Bolivia"},
    { 29, "br", "Brazil"},
    { 30, "bs", "Bahamas"},

    { 31, "bt", "Bhutan"},
    { 32, "bv", "Bouvet Island"},
    { 33, "bw", "Botswana"},
    { 34, "by", "Belarus"},
    { 35, "bz", "Belize"},
    { 36, "ca", "Canada"},
    { 37, "cc", "Cocos (Keeling) Islands"},
    { 38, "cd", "Democratic Republic of the Congo"},
    { 39, "cf", "Central African Republic"},
    { 40, "cg", "Congo"},

    { 41, "ch", "Switzerland"},
    { 42, "ci", "Ivory Coast (Cote d'Ivoire)"},
    { 43, "ck", "Cook Islands"},
    { 44, "cl", "Chile"},
    { 45, "cm", "Cameroon"},
    { 46, "cn", "China"},
    { 47, "co", "Colombia"},
    { 48, "cr", "Costa Rica"},
    { 49, "cs", "Czech Republic and Slovakia"},
    { 50, "cu", "Cuba"},

    { 61, "cv", "Cape Verde"},
    { 62, "cx", "Christmas Island"},
    { 63, "cy", "Cyprus"},
    { 64, "cz", "Czech Republic"},
    { 65, "de", "Germany"},
    { 66, "dj", "Djibouti"},
    { 67, "dk", "Denmark"},
    { 68, "dm", "Dominica"},
    { 69, "do", "Dominican Republic"},
    { 70, "dz", "Algeria"},

    { 81, "ec", "Ecuador"},
    { 82, "ee", "Estonia"},
    { 83, "eg", "Egypt"},
    { 84, "eh", "Western Sahara"},
    { 85, "er", "Eritrea"},
    { 86, "es", "Spain"},
    { 87, "et", "Ethiopia"},
    { 88, "fi", "Finland"},
    { 89, "fj", "Fiji"},
    { 90, "fk", "Falkland Islands"},

    { 91, "fm", "Micronesia"},
    { 92, "fo", "Faroe Islands"},
    { 93, "fr", "France"},
    { 94, "fx", "France (European Territory)"},
    { 95, "ga", "Gabon"},
    { 96, "gb", "United Kingdom"},
    { 97, "gd", "Grenada"},
    { 98, "ge", "Georgia"},
    { 99, "gf", "French Guyana"},
    {100, "gg", "Guernsey"},

    {101, "gh", "Ghana"},
    {102, "gi", "Gibraltar"},
    {103, "gl", "Greenland"},
    {104, "gm", "Gambia"},
    {105, "gn", "Guinea"},
    {106, "gp", "Guadeloupe (French)"},
    {107, "gq", "Equatorial Guinea"},
    {108, "gr", "Greece"},
    {109, "gs", "S. Georgia and S. Sandwich Isls."},
    {110, "gt", "Guatemala"},

    {111, "gu", "Guam (USA)"},
    {112, "gw", "Guinea Bissau"},
    {113, "gy", "Guyana"},
    {114, "hk", "Hong Kong"},
    {115, "hm", "Heard and McDonald Islands"},
    {116, "hn", "Honduras"},
    {117, "hr", "Croatia"},
    {118, "ht", "Haiti"},
    {119, "hu", "Hungary"},
    {120, "id", "Indonesia"},

    {121, "ie", "Ireland"},
    {122, "il", "Israel"},
    {123, "im", "Isle of Man"},
    {124, "in", "India"},
    {125, "io", "British Indian Ocean Territory"},
    {126, "iq", "Iraq"},
    {127, "ir", "Iran"},
    {128, "is", "Iceland"},
    {129, "it", "Italy"},
    {130, "je", "Jersey"},

    {131, "jm", "Jamaica"},
    {132, "jo", "Jordan"},
    {133, "jp", "Japan"},
    {134, "ke", "Kenya"},
    {135, "kg", "Kyrgyzstan"},
    {136, "kh", "Cambodia"},
    {137, "ki", "Kiribati"},
    {138, "km", "Comoros"},
    {139, "kn", "Saint Kitts & Nevis"},
    {140, "kp", "North Korea"},

    {141, "kr", "South Korea"},
    {142, "kw", "Kuwait"},
    {143, "ky", "Cayman Islands"},
    {144, "kz", "Kazakhstan"},
    {145, "la", "Laos"},
    {146, "lb", "Lebanon"},
    {147, "lc", "Saint Lucia"},
    {148, "li", "Liechtenstein"},
    {149, "lk", "Sri Lanka"},
    {150, "lr", "Liberia"},

    {151, "ls", "Lesotho"},
    {152, "lt", "Lithuania"},
    {153, "lu", "Luxembourg"},
    {154, "lv", "Latvia"},
    {155, "ly", "Libya"},
    {156, "ma", "Morocco"},
    {157, "mc", "Monaco"},
    {158, "md", "Moldova"},
    {159, "mg", "Madagascar"},
    {160, "mh", "Marshall Islands"},

    {161, "mk", "Macedonia"},
    {162, "ml", "Mali"},
    {163, "mm", "Myanmar"},
    {164, "mn", "Mongolia"},
    {165, "mo", "Macau"},
    {166, "mp", "Northern Mariana Islands"},
    {167, "mq", "Martinique (French)"},
    {168, "mr", "Mauritania"},
    {169, "ms", "Montserrat"},
    {170, "mt", "Malta"},

    {171, "mu", "Mauritius"},
    {172, "mv", "Maldives"},
    {173, "mw", "Malawi"},
    {174, "mx", "Mexico"},
    {175, "my", "Malaysia"},
    {176, "mz", "Mozambique"},
    {177, "na", "Namibia"},
    {178, "nc", "New Caledonia (French)"},
    {179, "ne", "Niger"},
    {180, "nf", "Norfolk Island"},

    {181, "ng", "Nigeria"},
    {182, "ni", "Nicaragua"},
    {183, "nl", "Netherlands"},
    {184, "no", "Norway"},
    {185, "np", "Nepal"},
    {186, "nr", "Nauru"},
    {187, "nu", "Niue"},
    {188, "nz", "New Zealand"},
    {189, "om", "Oman"},
    {190, "pa", "Panama"},

    {191, "pe", "Peru"},
    {192, "pf", "Polynesia (French)"},
    {193, "pg", "Papua New Guinea"},
    {194, "ph", "Philippines"},
    {195, "pk", "Pakistan"},
    {196, "pl", "Poland"},
    {197, "pm", "Saint Pierre and Miquelon"},
    {198, "pn", "Pitcairn Island"},
    {199, "pr", "Puerto Rico"},
    {200, "pt", "Portugal"},

    {201, "pw", "Palau"},
    {202, "py", "Paraguay"},
    {203, "qa", "Qatar"},
    {204, "re", "Reunion (French)"},
    {205, "ro", "Romania"},
    {206, "ru", "Russia"},
    {207, "rw", "Rwanda"},
    {208, "sa", "Saudi Arabia"},
    {209, "sb", "Solomon Islands"},
    {210, "sc", "Seychelles"},

    {211, "sd", "Sudan"},
    {212, "se", "Sweden"},
    {213, "sg", "Singapore"},
    {214, "sh", "Saint Helena"},
    {215, "si", "Slovenia"},
    {216, "sj", "Svalbard and Jan Mayen Islands"},
    {217, "sk", "Slovak Republic"},
    {218, "sl", "Sierra Leone"},
    {219, "sm", "San Marino"},
    {220, "sn", "Senegal"},

    {221, "so", "Somalia"},
    {222, "sr", "Suriname"},
    {223, "st", "Saint Tome and Principe"},
    {224, "su", "Former USSR"},
    {225, "sv", "El Salvador"},
    {226, "sy", "Syria"},
    {227, "sz", "Swaziland"},
    {228, "tc", "Turks and Caicos Islands"},
    {229, "td", "Chad"},
    {230, "tf", "French Southern Territories"},

    {231, "tg", "Togo"},
    {232, "th", "Thailand"},
    {233, "tj", "Tadjikistan"},
    {234, "tk", "Tokelau"},
    {235, "tm", "Turkmenistan"},
    {236, "tn", "Tunisia"},
    {237, "to", "Tonga"},
    {238, "tp", "East Timor"},
    {239, "tr", "Turkey"},
    {240, "tt", "Trinidad and Tobago"},

    {241, "tv", "Tuvalu"},
    {242, "tw", "Taiwan"},
    {243, "tz", "Tanzania"},
    {244, "ua", "Ukraine"},
    {245, "ug", "Uganda"},
    {246, "uk", "United Kingdom"},
    {247, "um", "USA Minor Outlying Islands"},
    {248, "us", "United States"},
    {249, "uy", "Uruguay"},
    {250, "uz", "Uzbekistan"},

    {251, "va", "Vatican City State"},
    {252, "vc", "Saint Vincent & Grenadines"},
    {253, "ve", "Venezuela"},
    {254, "vg", "Virgin Islands (British)"},
    {255, "vi", "Virgin Islands (USA)"},
    {256, "vn", "Vietnam"},
    {257, "vu", "Vanuatu"},
    {258, "wf", "Wallis and Futuna Islands"},
    {259, "ws", "Samoa"},
    {260, "ye", "Yemen"},

    {261, "yt", "Mayotte"},
    {262, "yu", "Yugoslavia"},
    {263, "za", "South Africa"},
    {264, "zm", "Zambia"},
    {265, "zr", "Democratic Republic of the Congo"},
    {266, "zw", "Zimbabwe"},
};

/* These aren't really countries, so we exclude them */
struct
{
    char *abbrev, *title;
}
us_domains[] =
{
    {"arpa", "Old style Arpanet"},
    {"com", "Commercial"},
    {"edu", "USA Educational"},
    {"gov", "USA Government"},
    {"int", "International"},
    {"mil", "USA Military"},
    {"net", "Network"},
    {"org", "Non-Profit Making Organisations"},
};

/* ---------------------------------------------------------- */

char *country_name (int c)
{
    /* printf ("looking up country name for %d\n", c); */
    return countries[find_country (c)].title;
}

/* ---------------------------------------------------------- */

char *country_abbrev (int c)
{
    /* printf ("looking up country abbrev for %d\n", c); */
    return countries[find_country (c)].abbrev;
}

/* ----------------------------------------------------------- */

int find_country (int country)
{
    int i;
    /* linear search */
    for (i=1; i<sizeof(countries)/sizeof(countries[0]); i++)
        if (country == countries[i].n) return i;
    return 0;
}

/* ----------------------------------------------------------- */

int find_abbrev (char *abbrev)
{
    int i;

    /* check common US domains first */
    for (i=0; i<sizeof(us_domains)/sizeof(us_domains[0]); i++)
    {
        if (strcmp (abbrev, us_domains[i].abbrev) == 0) return 248; /* assume USA */
    }

    /* linear search */
    for (i=1; i<sizeof(countries)/sizeof(countries[0]); i++)
        if (strcmp (abbrev, countries[i].abbrev) == 0) return countries[i].n;

    return 0;
}

/* ----------------------------------------------------------- */

int what_country (char *hostname)
{
    char *p;
    int  n;

    p = strrchr (hostname, '.');
    if (p == NULL) return 0;
    p++;
    n = find_abbrev (p);
    if (n == -1) return 0;
    return countries[n].n;
}
