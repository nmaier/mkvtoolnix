/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   ISO639 language definitions, lookup functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/iso639.h"
#include "common/strings/editing.h"
#include "common/strings/utf8.h"

const iso639_language_t iso639_languages[] = {
  { "Abkhazian",                                                                        "abk", "ab", NULL  },
  { "Achinese",                                                                         "ace", NULL, NULL  },
  { "Acoli",                                                                            "ach", NULL, NULL  },
  { "Adangme",                                                                          "ada", NULL, NULL  },
  { "Adyghe; Adygei",                                                                   "ady", NULL, NULL  },
  { "Afar",                                                                             "aar", "aa", NULL  },
  { "Afrihili",                                                                         "afh", NULL, NULL  },
  { "Afrikaans",                                                                        "afr", "af", NULL  },
  { "Afro-Asiatic languages",                                                           "afa", NULL, NULL  },
  { "Ainu",                                                                             "ain", NULL, NULL  },
  { "Akan",                                                                             "aka", "ak", NULL  },
  { "Akkadian",                                                                         "akk", NULL, NULL  },
  { "Albanian",                                                                         "alb", "sq", "sqi" },
  { "Aleut",                                                                            "ale", NULL, NULL  },
  { "Algonquian languages",                                                             "alg", NULL, NULL  },
  { "Altaic languages",                                                                 "tut", NULL, NULL  },
  { "Amharic",                                                                          "amh", "am", NULL  },
  { "Angika",                                                                           "anp", NULL, NULL  },
  { "Apache languages",                                                                 "apa", NULL, NULL  },
  { "Arabic",                                                                           "ara", "ar", NULL  },
  { "Aragonese",                                                                        "arg", "an", NULL  },
  { "Arapaho",                                                                          "arp", NULL, NULL  },
  { "Arawak",                                                                           "arw", NULL, NULL  },
  { "Armenian",                                                                         "arm", "hy", "hye" },
  { "Aromanian; Arumanian; Macedo-Romanian",                                            "rup", NULL, NULL  },
  { "Artificial languages",                                                             "art", NULL, NULL  },
  { "Assamese",                                                                         "asm", "as", NULL  },
  { "Asturian; Bable; Leonese; Asturleonese",                                           "ast", NULL, NULL  },
  { "Athapascan languages",                                                             "ath", NULL, NULL  },
  { "Australian languages",                                                             "aus", NULL, NULL  },
  { "Austronesian languages",                                                           "map", NULL, NULL  },
  { "Avaric",                                                                           "ava", "av", NULL  },
  { "Avestan",                                                                          "ave", "ae", NULL  },
  { "Awadhi",                                                                           "awa", NULL, NULL  },
  { "Aymara",                                                                           "aym", "ay", NULL  },
  { "Azerbaijani",                                                                      "aze", "az", NULL  },
  { "Balinese",                                                                         "ban", NULL, NULL  },
  { "Baltic languages",                                                                 "bat", NULL, NULL  },
  { "Baluchi",                                                                          "bal", NULL, NULL  },
  { "Bambara",                                                                          "bam", "bm", NULL  },
  { "Bamileke languages",                                                               "bai", NULL, NULL  },
  { "Banda languages",                                                                  "bad", NULL, NULL  },
  { "Bantu (Other)",                                                                    "bnt", NULL, NULL  },
  { "Basa",                                                                             "bas", NULL, NULL  },
  { "Bashkir",                                                                          "bak", "ba", NULL  },
  { "Basque",                                                                           "baq", "eu", "eus" },
  { "Batak languages",                                                                  "btk", NULL, NULL  },
  { "Beja; Bedawiyet",                                                                  "bej", NULL, NULL  },
  { "Belarusian",                                                                       "bel", "be", NULL  },
  { "Bemba",                                                                            "bem", NULL, NULL  },
  { "Bengali",                                                                          "ben", "bn", NULL  },
  { "Berber languages",                                                                 "ber", NULL, NULL  },
  { "Bhojpuri",                                                                         "bho", NULL, NULL  },
  { "Bihari languages",                                                                 "bih", "bh", NULL  },
  { "Bikol",                                                                            "bik", NULL, NULL  },
  { "Bini; Edo",                                                                        "bin", NULL, NULL  },
  { "Bislama",                                                                          "bis", "bi", NULL  },
  { "Blin; Bilin",                                                                      "byn", NULL, NULL  },
  { "Blissymbols; Blissymbolics; Bliss",                                                "zbl", NULL, NULL  },
  { "Bokmål, Norwegian; Norwegian Bokmål",                                              "nob", "nb", NULL  },
  { "Bosnian",                                                                          "bos", "bs", NULL  },
  { "Braj",                                                                             "bra", NULL, NULL  },
  { "Breton",                                                                           "bre", "br", NULL  },
  { "Buginese",                                                                         "bug", NULL, NULL  },
  { "Bulgarian",                                                                        "bul", "bg", NULL  },
  { "Buriat",                                                                           "bua", NULL, NULL  },
  { "Burmese",                                                                          "bur", "my", "mya" },
  { "Caddo",                                                                            "cad", NULL, NULL  },
  { "Catalan; Valencian",                                                               "cat", "ca", NULL  },
  { "Caucasian languages",                                                              "cau", NULL, NULL  },
  { "Cebuano",                                                                          "ceb", NULL, NULL  },
  { "Celtic languages",                                                                 "cel", NULL, NULL  },
  { "Central American Indian languages",                                                "cai", NULL, NULL  },
  { "Central Khmer",                                                                    "khm", "km", NULL  },
  { "Chagatai",                                                                         "chg", NULL, NULL  },
  { "Chamic languages",                                                                 "cmc", NULL, NULL  },
  { "Chamorro",                                                                         "cha", "ch", NULL  },
  { "Chechen",                                                                          "che", "ce", NULL  },
  { "Cherokee",                                                                         "chr", NULL, NULL  },
  { "Cheyenne",                                                                         "chy", NULL, NULL  },
  { "Chibcha",                                                                          "chb", NULL, NULL  },
  { "Chichewa; Chewa; Nyanja",                                                          "nya", "ny", NULL  },
  { "Chinese",                                                                          "chi", "zh", "zho" },
  { "Chinook jargon",                                                                   "chn", NULL, NULL  },
  { "Chipewyan; Dene Suline",                                                           "chp", NULL, NULL  },
  { "Choctaw",                                                                          "cho", NULL, NULL  },
  { "Church Slavic; Old Slavonic; Church Slavonic; Old Bulgarian; Old Church Slavonic", "chu", "cu", NULL  },
  { "Chuukese",                                                                         "chk", NULL, NULL  },
  { "Chuvash",                                                                          "chv", "cv", NULL  },
  { "Classical Newari; Old Newari; Classical Nepal Bhasa",                              "nwc", NULL, NULL  },
  { "Classical Syriac",                                                                 "syc", NULL, NULL  },
  { "Coptic",                                                                           "cop", NULL, NULL  },
  { "Cornish",                                                                          "cor", "kw", NULL  },
  { "Corsican",                                                                         "cos", "co", NULL  },
  { "Cree",                                                                             "cre", "cr", NULL  },
  { "Creek",                                                                            "mus", NULL, NULL  },
  { "Creoles and pidgins",                                                              "crp", NULL, NULL  },
  { "Creoles and pidgins, English based",                                               "cpe", NULL, NULL  },
  { "Creoles and pidgins, French-based",                                                "cpf", NULL, NULL  },
  { "Creoles and pidgins, Portuguese-based",                                            "cpp", NULL, NULL  },
  { "Crimean Tatar; Crimean Turkish",                                                   "crh", NULL, NULL  },
  { "Croatian",                                                                         "hrv", "hr", NULL  },
  { "Cushitic languages",                                                               "cus", NULL, NULL  },
  { "Czech",                                                                            "cze", "cs", "ces" },
  { "Dakota",                                                                           "dak", NULL, NULL  },
  { "Danish",                                                                           "dan", "da", NULL  },
  { "Dargwa",                                                                           "dar", NULL, NULL  },
  { "Delaware",                                                                         "del", NULL, NULL  },
  { "Dinka",                                                                            "din", NULL, NULL  },
  { "Divehi; Dhivehi; Maldivian",                                                       "div", "dv", NULL  },
  { "Dogri",                                                                            "doi", NULL, NULL  },
  { "Dogrib",                                                                           "dgr", NULL, NULL  },
  { "Dravidian languages",                                                              "dra", NULL, NULL  },
  { "Duala",                                                                            "dua", NULL, NULL  },
  { "Dutch, Middle (ca.1050-1350)",                                                     "dum", NULL, NULL  },
  { "Dutch; Flemish",                                                                   "dut", "nl", "nld" },
  { "Dyula",                                                                            "dyu", NULL, NULL  },
  { "Dzongkha",                                                                         "dzo", "dz", NULL  },
  { "Eastern Frisian",                                                                  "frs", NULL, NULL  },
  { "Efik",                                                                             "efi", NULL, NULL  },
  { "Egyptian (Ancient)",                                                               "egy", NULL, NULL  },
  { "Ekajuk",                                                                           "eka", NULL, NULL  },
  { "Elamite",                                                                          "elx", NULL, NULL  },
  { "English",                                                                          "eng", "en", NULL  },
  { "English, Middle (1100-1500)",                                                      "enm", NULL, NULL  },
  { "English, Old (ca.450-1100)",                                                       "ang", NULL, NULL  },
  { "Erzya",                                                                            "myv", NULL, NULL  },
  { "Esperanto",                                                                        "epo", "eo", NULL  },
  { "Estonian",                                                                         "est", "et", NULL  },
  { "Ewe",                                                                              "ewe", "ee", NULL  },
  { "Ewondo",                                                                           "ewo", NULL, NULL  },
  { "Fang",                                                                             "fan", NULL, NULL  },
  { "Fanti",                                                                            "fat", NULL, NULL  },
  { "Faroese",                                                                          "fao", "fo", NULL  },
  { "Fijian",                                                                           "fij", "fj", NULL  },
  { "Filipino; Pilipino",                                                               "fil", NULL, NULL  },
  { "Finnish",                                                                          "fin", "fi", NULL  },
  { "Finno-Ugrian languages",                                                           "fiu", NULL, NULL  },
  { "Fon",                                                                              "fon", NULL, NULL  },
  { "French",                                                                           "fre", "fr", "fra" },
  { "French, Middle (ca.1400-1600)",                                                    "frm", NULL, NULL  },
  { "French, Old (842-ca.1400)",                                                        "fro", NULL, NULL  },
  { "Friulian",                                                                         "fur", NULL, NULL  },
  { "Fulah",                                                                            "ful", "ff", NULL  },
  { "Ga",                                                                               "gaa", NULL, NULL  },
  { "Gaelic; Scottish Gaelic",                                                          "gla", "gd", NULL  },
  { "Galibi Carib",                                                                     "car", NULL, NULL  },
  { "Galician",                                                                         "glg", "gl", NULL  },
  { "Ganda",                                                                            "lug", "lg", NULL  },
  { "Gayo",                                                                             "gay", NULL, NULL  },
  { "Gbaya",                                                                            "gba", NULL, NULL  },
  { "Geez",                                                                             "gez", NULL, NULL  },
  { "Georgian",                                                                         "geo", "ka", "kat" },
  { "German",                                                                           "ger", "de", "deu" },
  { "German, Middle High (ca.1050-1500)",                                               "gmh", NULL, NULL  },
  { "German, Old High (ca.750-1050)",                                                   "goh", NULL, NULL  },
  { "Germanic languages",                                                               "gem", NULL, NULL  },
  { "Gilbertese",                                                                       "gil", NULL, NULL  },
  { "Gondi",                                                                            "gon", NULL, NULL  },
  { "Gorontalo",                                                                        "gor", NULL, NULL  },
  { "Gothic",                                                                           "got", NULL, NULL  },
  { "Grebo",                                                                            "grb", NULL, NULL  },
  { "Greek, Ancient (to 1453)",                                                         "grc", NULL, NULL  },
  { "Greek, Modern (1453-)",                                                            "gre", "el", "ell" },
  { "Guarani",                                                                          "grn", "gn", NULL  },
  { "Gujarati",                                                                         "guj", "gu", NULL  },
  { "Gwich'in",                                                                         "gwi", NULL, NULL  },
  { "Haida",                                                                            "hai", NULL, NULL  },
  { "Haitian; Haitian Creole",                                                          "hat", "ht", NULL  },
  { "Hausa",                                                                            "hau", "ha", NULL  },
  { "Hawaiian",                                                                         "haw", NULL, NULL  },
  { "Hebrew",                                                                           "heb", "he", NULL  },
  { "Herero",                                                                           "her", "hz", NULL  },
  { "Hiligaynon",                                                                       "hil", NULL, NULL  },
  { "Himachali languages; Western Pahari languages",                                    "him", NULL, NULL  },
  { "Hindi",                                                                            "hin", "hi", NULL  },
  { "Hiri Motu",                                                                        "hmo", "ho", NULL  },
  { "Hittite",                                                                          "hit", NULL, NULL  },
  { "Hmong; Mong",                                                                      "hmn", NULL, NULL  },
  { "Hungarian",                                                                        "hun", "hu", NULL  },
  { "Hupa",                                                                             "hup", NULL, NULL  },
  { "Iban",                                                                             "iba", NULL, NULL  },
  { "Icelandic",                                                                        "ice", "is", "isl" },
  { "Ido",                                                                              "ido", "io", NULL  },
  { "Igbo",                                                                             "ibo", "ig", NULL  },
  { "Ijo languages",                                                                    "ijo", NULL, NULL  },
  { "Iloko",                                                                            "ilo", NULL, NULL  },
  { "Inari Sami",                                                                       "smn", NULL, NULL  },
  { "Indic languages",                                                                  "inc", NULL, NULL  },
  { "Indo-European languages",                                                          "ine", NULL, NULL  },
  { "Indonesian",                                                                       "ind", "id", NULL  },
  { "Ingush",                                                                           "inh", NULL, NULL  },
  { "Interlingua (International Auxiliary Language Association)",                       "ina", "ia", NULL  },
  { "Interlingue; Occidental",                                                          "ile", "ie", NULL  },
  { "Inuktitut",                                                                        "iku", "iu", NULL  },
  { "Inupiaq",                                                                          "ipk", "ik", NULL  },
  { "Iranian languages",                                                                "ira", NULL, NULL  },
  { "Irish",                                                                            "gle", "ga", NULL  },
  { "Irish, Middle (900-1200)",                                                         "mga", NULL, NULL  },
  { "Irish, Old (to 900)",                                                              "sga", NULL, NULL  },
  { "Iroquoian languages",                                                              "iro", NULL, NULL  },
  { "Italian",                                                                          "ita", "it", NULL  },
  { "Japanese",                                                                         "jpn", "ja", NULL  },
  { "Javanese",                                                                         "jav", "jv", NULL  },
  { "Judeo-Arabic",                                                                     "jrb", NULL, NULL  },
  { "Judeo-Persian",                                                                    "jpr", NULL, NULL  },
  { "Kabardian",                                                                        "kbd", NULL, NULL  },
  { "Kabyle",                                                                           "kab", NULL, NULL  },
  { "Kachin; Jingpho",                                                                  "kac", NULL, NULL  },
  { "Kalaallisut; Greenlandic",                                                         "kal", "kl", NULL  },
  { "Kalmyk; Oirat",                                                                    "xal", NULL, NULL  },
  { "Kamba",                                                                            "kam", NULL, NULL  },
  { "Kannada",                                                                          "kan", "kn", NULL  },
  { "Kanuri",                                                                           "kau", "kr", NULL  },
  { "Kara-Kalpak",                                                                      "kaa", NULL, NULL  },
  { "Karachay-Balkar",                                                                  "krc", NULL, NULL  },
  { "Karelian",                                                                         "krl", NULL, NULL  },
  { "Karen languages",                                                                  "kar", NULL, NULL  },
  { "Kashmiri",                                                                         "kas", "ks", NULL  },
  { "Kashubian",                                                                        "csb", NULL, NULL  },
  { "Kawi",                                                                             "kaw", NULL, NULL  },
  { "Kazakh",                                                                           "kaz", "kk", NULL  },
  { "Khasi",                                                                            "kha", NULL, NULL  },
  { "Khoisan languages",                                                                "khi", NULL, NULL  },
  { "Khotanese; Sakan",                                                                 "kho", NULL, NULL  },
  { "Kikuyu; Gikuyu",                                                                   "kik", "ki", NULL  },
  { "Kimbundu",                                                                         "kmb", NULL, NULL  },
  { "Kinyarwanda",                                                                      "kin", "rw", NULL  },
  { "Kirghiz; Kyrgyz",                                                                  "kir", "ky", NULL  },
  { "Klingon; tlhIngan-Hol",                                                            "tlh", NULL, NULL  },
  { "Komi",                                                                             "kom", "kv", NULL  },
  { "Kongo",                                                                            "kon", "kg", NULL  },
  { "Konkani",                                                                          "kok", NULL, NULL  },
  { "Korean",                                                                           "kor", "ko", NULL  },
  { "Kosraean",                                                                         "kos", NULL, NULL  },
  { "Kpelle",                                                                           "kpe", NULL, NULL  },
  { "Kru languages",                                                                    "kro", NULL, NULL  },
  { "Kuanyama; Kwanyama",                                                               "kua", "kj", NULL  },
  { "Kumyk",                                                                            "kum", NULL, NULL  },
  { "Kurdish",                                                                          "kur", "ku", NULL  },
  { "Kurukh",                                                                           "kru", NULL, NULL  },
  { "Kutenai",                                                                          "kut", NULL, NULL  },
  { "Ladino",                                                                           "lad", NULL, NULL  },
  { "Lahnda",                                                                           "lah", NULL, NULL  },
  { "Lamba",                                                                            "lam", NULL, NULL  },
  { "Land Dayak languages",                                                             "day", NULL, NULL  },
  { "Lao",                                                                              "lao", "lo", NULL  },
  { "Latin",                                                                            "lat", "la", NULL  },
  { "Latvian",                                                                          "lav", "lv", NULL  },
  { "Lezghian",                                                                         "lez", NULL, NULL  },
  { "Limburgan; Limburger; Limburgish",                                                 "lim", "li", NULL  },
  { "Lingala",                                                                          "lin", "ln", NULL  },
  { "Lithuanian",                                                                       "lit", "lt", NULL  },
  { "Lojban",                                                                           "jbo", NULL, NULL  },
  { "Low German; Low Saxon; German, Low; Saxon, Low",                                   "nds", NULL, NULL  },
  { "Lower Sorbian",                                                                    "dsb", NULL, NULL  },
  { "Lozi",                                                                             "loz", NULL, NULL  },
  { "Luba-Katanga",                                                                     "lub", "lu", NULL  },
  { "Luba-Lulua",                                                                       "lua", NULL, NULL  },
  { "Luiseno",                                                                          "lui", NULL, NULL  },
  { "Lule Sami",                                                                        "smj", NULL, NULL  },
  { "Lunda",                                                                            "lun", NULL, NULL  },
  { "Luo (Kenya and Tanzania)",                                                         "luo", NULL, NULL  },
  { "Lushai",                                                                           "lus", NULL, NULL  },
  { "Luxembourgish; Letzeburgesch",                                                     "ltz", "lb", NULL  },
  { "Macedonian",                                                                       "mac", "mk", "mkd" },
  { "Madurese",                                                                         "mad", NULL, NULL  },
  { "Magahi",                                                                           "mag", NULL, NULL  },
  { "Maithili",                                                                         "mai", NULL, NULL  },
  { "Makasar",                                                                          "mak", NULL, NULL  },
  { "Malagasy",                                                                         "mlg", "mg", NULL  },
  { "Malay",                                                                            "may", "ms", "msa" },
  { "Malayalam",                                                                        "mal", "ml", NULL  },
  { "Maltese",                                                                          "mlt", "mt", NULL  },
  { "Manchu",                                                                           "mnc", NULL, NULL  },
  { "Mandar",                                                                           "mdr", NULL, NULL  },
  { "Mandingo",                                                                         "man", NULL, NULL  },
  { "Manipuri",                                                                         "mni", NULL, NULL  },
  { "Manobo languages",                                                                 "mno", NULL, NULL  },
  { "Manx",                                                                             "glv", "gv", NULL  },
  { "Maori",                                                                            "mao", "mi", "mri" },
  { "Mapudungun; Mapuche",                                                              "arn", NULL, NULL  },
  { "Marathi",                                                                          "mar", "mr", NULL  },
  { "Mari",                                                                             "chm", NULL, NULL  },
  { "Marshallese",                                                                      "mah", "mh", NULL  },
  { "Marwari",                                                                          "mwr", NULL, NULL  },
  { "Masai",                                                                            "mas", NULL, NULL  },
  { "Mayan languages",                                                                  "myn", NULL, NULL  },
  { "Mende",                                                                            "men", NULL, NULL  },
  { "Mi'kmaq; Micmac",                                                                  "mic", NULL, NULL  },
  { "Minangkabau",                                                                      "min", NULL, NULL  },
  { "Mirandese",                                                                        "mwl", NULL, NULL  },
  { "Mohawk",                                                                           "moh", NULL, NULL  },
  { "Moksha",                                                                           "mdf", NULL, NULL  },
  { "Mon-Khmer languages",                                                              "mkh", NULL, NULL  },
  { "Mongo",                                                                            "lol", NULL, NULL  },
  { "Mongolian",                                                                        "mon", "mn", NULL  },
  { "Mossi",                                                                            "mos", NULL, NULL  },
  { "Multiple languages",                                                               "mul", NULL, NULL  },
  { "Munda languages",                                                                  "mun", NULL, NULL  },
  { "N'Ko",                                                                             "nqo", NULL, NULL  },
  { "Nahuatl languages",                                                                "nah", NULL, NULL  },
  { "Nauru",                                                                            "nau", "na", NULL  },
  { "Navajo; Navaho",                                                                   "nav", "nv", NULL  },
  { "Ndebele, North; North Ndebele",                                                    "nde", "nd", NULL  },
  { "Ndebele, South; South Ndebele",                                                    "nbl", "nr", NULL  },
  { "Ndonga",                                                                           "ndo", "ng", NULL  },
  { "Neapolitan",                                                                       "nap", NULL, NULL  },
  { "Nepal Bhasa; Newari",                                                              "new", NULL, NULL  },
  { "Nepali",                                                                           "nep", "ne", NULL  },
  { "Nias",                                                                             "nia", NULL, NULL  },
  { "Niger-Kordofanian languages",                                                      "nic", NULL, NULL  },
  { "Nilo-Saharan languages",                                                           "ssa", NULL, NULL  },
  { "Niuean",                                                                           "niu", NULL, NULL  },
  { "No linguistic content; Not applicable",                                            "zxx", NULL, NULL  },
  { "Nogai",                                                                            "nog", NULL, NULL  },
  { "Norse, Old",                                                                       "non", NULL, NULL  },
  { "North American Indian languages",                                                  "nai", NULL, NULL  },
  { "Northern Frisian",                                                                 "frr", NULL, NULL  },
  { "Northern Sami",                                                                    "sme", "se", NULL  },
  { "Norwegian Nynorsk; Nynorsk, Norwegian",                                            "nno", "nn", NULL  },
  { "Norwegian",                                                                        "nor", "no", NULL  },
  { "Nubian languages",                                                                 "nub", NULL, NULL  },
  { "Nyamwezi",                                                                         "nym", NULL, NULL  },
  { "Nyankole",                                                                         "nyn", NULL, NULL  },
  { "Nyoro",                                                                            "nyo", NULL, NULL  },
  { "Nzima",                                                                            "nzi", NULL, NULL  },
  { "Occitan (post 1500)",                                                              "oci", "oc", NULL  },
  { "Official Aramaic (700-300 BCE); Imperial Aramaic (700-300 BCE)",                   "arc", NULL, NULL  },
  { "Ojibwa",                                                                           "oji", "oj", NULL  },
  { "Oriya",                                                                            "ori", "or", NULL  },
  { "Oromo",                                                                            "orm", "om", NULL  },
  { "Osage",                                                                            "osa", NULL, NULL  },
  { "Ossetian; Ossetic",                                                                "oss", "os", NULL  },
  { "Otomian languages",                                                                "oto", NULL, NULL  },
  { "Pahlavi",                                                                          "pal", NULL, NULL  },
  { "Palauan",                                                                          "pau", NULL, NULL  },
  { "Pali",                                                                             "pli", "pi", NULL  },
  { "Pampanga; Kapampangan",                                                            "pam", NULL, NULL  },
  { "Pangasinan",                                                                       "pag", NULL, NULL  },
  { "Panjabi; Punjabi",                                                                 "pan", "pa", NULL  },
  { "Papiamento",                                                                       "pap", NULL, NULL  },
  { "Papuan languages",                                                                 "paa", NULL, NULL  },
  { "Pedi; Sepedi; Northern Sotho",                                                     "nso", NULL, NULL  },
  { "Persian",                                                                          "per", "fa", "fas" },
  { "Persian, Old (ca.600-400 B.C.)",                                                   "peo", NULL, NULL  },
  { "Philippine languages",                                                             "phi", NULL, NULL  },
  { "Phoenician",                                                                       "phn", NULL, NULL  },
  { "Pohnpeian",                                                                        "pon", NULL, NULL  },
  { "Polish",                                                                           "pol", "pl", NULL  },
  { "Portuguese",                                                                       "por", "pt", NULL  },
  { "Prakrit languages",                                                                "pra", NULL, NULL  },
  { "Provençal, Old (to 1500);Occitan, Old (to 1500)",                                  "pro", NULL, NULL  },
  { "Pushto; Pashto",                                                                   "pus", "ps", NULL  },
  { "Quechua",                                                                          "que", "qu", NULL  },
  { "Rajasthani",                                                                       "raj", NULL, NULL  },
  { "Rapanui",                                                                          "rap", NULL, NULL  },
  { "Rarotongan; Cook Islands Maori",                                                   "rar", NULL, NULL  },
  { "Romance languages",                                                                "roa", NULL, NULL  },
  { "Romanian; Moldavian; Moldovan",                                                    "rum", "ro", "ron" },
  { "Romansh",                                                                          "roh", "rm", NULL  },
  { "Romany",                                                                           "rom", NULL, NULL  },
  { "Rundi",                                                                            "run", "rn", NULL  },
  { "Russian",                                                                          "rus", "ru", NULL  },
  { "Salishan languages",                                                               "sal", NULL, NULL  },
  { "Samaritan Aramaic",                                                                "sam", NULL, NULL  },
  { "Sami languages",                                                                   "smi", NULL, NULL  },
  { "Samoan",                                                                           "smo", "sm", NULL  },
  { "Sandawe",                                                                          "sad", NULL, NULL  },
  { "Sango",                                                                            "sag", "sg", NULL  },
  { "Sanskrit",                                                                         "san", "sa", NULL  },
  { "Santali",                                                                          "sat", NULL, NULL  },
  { "Sardinian",                                                                        "srd", "sc", NULL  },
  { "Sasak",                                                                            "sas", NULL, NULL  },
  { "Scots",                                                                            "sco", NULL, NULL  },
  { "Selkup",                                                                           "sel", NULL, NULL  },
  { "Semitic languages",                                                                "sem", NULL, NULL  },
  { "Serbian",                                                                          "srp", "sr", NULL  },
  { "Serer",                                                                            "srr", NULL, NULL  },
  { "Shan",                                                                             "shn", NULL, NULL  },
  { "Shona",                                                                            "sna", "sn", NULL  },
  { "Sichuan Yi; Nuosu",                                                                "iii", "ii", NULL  },
  { "Sicilian",                                                                         "scn", NULL, NULL  },
  { "Sidamo",                                                                           "sid", NULL, NULL  },
  { "Sign Languages",                                                                   "sgn", NULL, NULL  },
  { "Siksika",                                                                          "bla", NULL, NULL  },
  { "Sindhi",                                                                           "snd", "sd", NULL  },
  { "Sinhala; Sinhalese",                                                               "sin", "si", NULL  },
  { "Sino-Tibetan languages",                                                           "sit", NULL, NULL  },
  { "Siouan languages",                                                                 "sio", NULL, NULL  },
  { "Skolt Sami",                                                                       "sms", NULL, NULL  },
  { "Slave (Athapascan)",                                                               "den", NULL, NULL  },
  { "Slavic languages",                                                                 "sla", NULL, NULL  },
  { "Slovak",                                                                           "slo", "sk", "slk" },
  { "Slovenian",                                                                        "slv", "sl", NULL  },
  { "Sogdian",                                                                          "sog", NULL, NULL  },
  { "Somali",                                                                           "som", "so", NULL  },
  { "Songhai languages",                                                                "son", NULL, NULL  },
  { "Soninke",                                                                          "snk", NULL, NULL  },
  { "Sorbian languages",                                                                "wen", NULL, NULL  },
  { "Sotho, Southern",                                                                  "sot", "st", NULL  },
  { "South American Indian (Other)",                                                    "sai", NULL, NULL  },
  { "Southern Altai",                                                                   "alt", NULL, NULL  },
  { "Southern Sami",                                                                    "sma", NULL, NULL  },
  { "Spanish; Castillan",                                                               "spa", "es", NULL  },
  { "Sranan Tongo",                                                                     "srn", NULL, NULL  },
  { "Sukuma",                                                                           "suk", NULL, NULL  },
  { "Sumerian",                                                                         "sux", NULL, NULL  },
  { "Sundanese",                                                                        "sun", "su", NULL  },
  { "Susu",                                                                             "sus", NULL, NULL  },
  { "Swahili",                                                                          "swa", "sw", NULL  },
  { "Swati",                                                                            "ssw", "ss", NULL  },
  { "Swedish",                                                                          "swe", "sv", NULL  },
  { "Swiss German; Alemannic; Alsatian",                                                "gsw", NULL, NULL  },
  { "Syriac",                                                                           "syr", NULL, NULL  },
  { "Tagalog",                                                                          "tgl", "tl", NULL  },
  { "Tahitian",                                                                         "tah", "ty", NULL  },
  { "Tai languages",                                                                    "tai", NULL, NULL  },
  { "Tajik",                                                                            "tgk", "tg", NULL  },
  { "Tamashek",                                                                         "tmh", NULL, NULL  },
  { "Tamil",                                                                            "tam", "ta", NULL  },
  { "Tatar",                                                                            "tat", "tt", NULL  },
  { "Telugu",                                                                           "tel", "te", NULL  },
  { "Tereno",                                                                           "ter", NULL, NULL  },
  { "Tetum",                                                                            "tet", NULL, NULL  },
  { "Thai",                                                                             "tha", "th", NULL  },
  { "Tibetan",                                                                          "tib", "bo", "bod" },
  { "Tigre",                                                                            "tig", NULL, NULL  },
  { "Tigrinya",                                                                         "tir", "ti", NULL  },
  { "Timne",                                                                            "tem", NULL, NULL  },
  { "Tiv",                                                                              "tiv", NULL, NULL  },
  { "Tlingit",                                                                          "tli", NULL, NULL  },
  { "Tok Pisin",                                                                        "tpi", NULL, NULL  },
  { "Tokelau",                                                                          "tkl", NULL, NULL  },
  { "Tonga (Nyasa)",                                                                    "tog", NULL, NULL  },
  { "Tonga (Tonga Islands)",                                                            "ton", "to", NULL  },
  { "Tsimshian",                                                                        "tsi", NULL, NULL  },
  { "Tsonga",                                                                           "tso", "ts", NULL  },
  { "Tswana",                                                                           "tsn", "tn", NULL  },
  { "Tumbuka",                                                                          "tum", NULL, NULL  },
  { "Tupi languages",                                                                   "tup", NULL, NULL  },
  { "Turkish",                                                                          "tur", "tr", NULL  },
  { "Turkish, Ottoman (1500-1928)",                                                     "ota", NULL, NULL  },
  { "Turkmen",                                                                          "tuk", "tk", NULL  },
  { "Tuvalu",                                                                           "tvl", NULL, NULL  },
  { "Tuvinian",                                                                         "tyv", NULL, NULL  },
  { "Twi",                                                                              "twi", "tw", NULL  },
  { "Udmurt",                                                                           "udm", NULL, NULL  },
  { "Ugaritic",                                                                         "uga", NULL, NULL  },
  { "Uighur; Uyghur",                                                                   "uig", "ug", NULL  },
  { "Ukrainian",                                                                        "ukr", "uk", NULL  },
  { "Umbundu",                                                                          "umb", NULL, NULL  },
  { "Uncoded languages",                                                                "mis", NULL, NULL  },
  { "Undetermined",                                                                     "und", NULL, NULL  },
  { "Upper Sorbian",                                                                    "hsb", NULL, NULL  },
  { "Urdu",                                                                             "urd", "ur", NULL  },
  { "Uzbek",                                                                            "uzb", "uz", NULL  },
  { "Vai",                                                                              "vai", NULL, NULL  },
  { "Venda",                                                                            "ven", "ve", NULL  },
  { "Vietnamese",                                                                       "vie", "vi", NULL  },
  { "Volapük",                                                                          "vol", "vo", NULL  },
  { "Votic",                                                                            "vot", NULL, NULL  },
  { "Wakashan languages",                                                               "wak", NULL, NULL  },
  { "Walamo",                                                                           "wal", NULL, NULL  },
  { "Walloon",                                                                          "wln", "wa", NULL  },
  { "Waray",                                                                            "war", NULL, NULL  },
  { "Washo",                                                                            "was", NULL, NULL  },
  { "Welsh",                                                                            "wel", "cy", "cym" },
  { "Western Frisian",                                                                  "fry", "fy", NULL  },
  { "Wolof",                                                                            "wol", "wo", NULL  },
  { "Xhosa",                                                                            "xho", "xh", NULL  },
  { "Yakut",                                                                            "sah", NULL, NULL  },
  { "Yao",                                                                              "yao", NULL, NULL  },
  { "Yapese",                                                                           "yap", NULL, NULL  },
  { "Yiddish",                                                                          "yid", "yi", NULL  },
  { "Yoruba",                                                                           "yor", "yo", NULL  },
  { "Yupik languages",                                                                  "ypk", NULL, NULL  },
  { "Zande languages",                                                                  "znd", NULL, NULL  },
  { "Zapotec",                                                                          "zap", NULL, NULL  },
  { "Zaza; Dimili; Dimli; Kirdki; Kirmanjki; Zazaki",                                   "zza", NULL, NULL  },
  { "Zenaga",                                                                           "zen", NULL, NULL  },
  { "Zhuang; Chuang",                                                                   "zha", "za", NULL  },
  { "Zulu",                                                                             "zul", "zu", NULL  },
  { "Zuni",                                                                             "zun", NULL, NULL  },
  { NULL,                                                                               NULL,  NULL, NULL  },
};

bool
is_valid_iso639_2_code(const char *iso639_2_code) {
  int i = 0;
  while (iso639_languages[i].iso639_2_code != NULL) {
    if (!strcmp(iso639_languages[i].iso639_2_code, iso639_2_code))
      return true;
    i++;
  }

  return false;
}

#define FILL(s, idx) s + std::wstring(longest[idx] - get_width_in_em(s), L' ')

void
list_iso639_languages() {
  std::wstring w_col1 = to_wide(Y("English language name"));
  std::wstring w_col2 = to_wide(Y("ISO639-2 code"));
  std::wstring w_col3 = to_wide(Y("ISO639-1 code"));

  size_t longest[3]   = { get_width_in_em(w_col1), get_width_in_em(w_col2), get_width_in_em(w_col3) };
  int i;

  for (i = 0; NULL != iso639_languages[i].iso639_2_code; ++i) {
    longest[0] = std::max(longest[0], get_width_in_em(to_wide(iso639_languages[i].english_name)));
    longest[1] = std::max(longest[1], get_width_in_em(to_wide(iso639_languages[i].iso639_2_code)));
    longest[2] = std::max(longest[2], get_width_in_em(to_wide(NULL != iso639_languages[i].iso639_1_code ? iso639_languages[i].iso639_1_code : "")));
  }

  mxinfo(FILL(w_col1, 0) + L" | " + FILL(w_col2, 1) + L" | " + FILL(w_col3, 2) + L"\n");
  mxinfo(std::wstring(longest[0] + 1, L'-') + L'+' + std::wstring(longest[1] + 2, L'-') + L'+' + std::wstring(longest[2] + 1, L'-') + L"\n");

  for (i = 0; NULL != iso639_languages[i].iso639_2_code; ++i) {
    std::wstring english = to_wide(iso639_languages[i].english_name);
    std::wstring code2   = to_wide(iso639_languages[i].iso639_2_code);
    std::wstring code1   = NULL != iso639_languages[i].iso639_1_code ? to_wide(iso639_languages[i].iso639_1_code) : L"";
    mxinfo(FILL(english, 0) + L" | " + FILL(code2, 1) + L" | " + FILL(code1, 2) + L"\n");
  }
}

const char *
map_iso639_2_to_iso639_1(const char *iso639_2_code) {
  uint32_t i;

  for (i = 0; iso639_languages[i].iso639_2_code != NULL; i++)
    if (!strcmp(iso639_2_code, iso639_languages[i].iso639_2_code))
      return iso639_languages[i].iso639_1_code;

  return NULL;
}

bool
is_popular_language(const char *lang) {
  return !strcmp(lang, "Chinese")
      || !strcmp(lang, "Dutch")
      || !strcmp(lang, "English")
      || !strcmp(lang, "Finnish")
      || !strcmp(lang, "French")
      || !strcmp(lang, "German")
      || !strcmp(lang, "Italian")
      || !strcmp(lang, "Japanese")
      || !strcmp(lang, "Norwegian")
      || !strcmp(lang, "Portuguese")
      || !strcmp(lang, "Russian")
      || !strcmp(lang, "Spanish")
      || !strcmp(lang, "Spanish; Castillan")
      || !strcmp(lang, "Swedish")
    ;
}

bool
is_popular_language_code(const char *code) {
  return !strcmp(code, "chi") // Chinese
      || !strcmp(code, "dut") // Dutch
      || !strcmp(code, "eng") // English
      || !strcmp(code, "fin") // Finnish
      || !strcmp(code, "fre") // French
      || !strcmp(code, "ger") // German
      || !strcmp(code, "ita") // Italian
      || !strcmp(code, "jpn") // Japanese
      || !strcmp(code, "nor") // Norwegian
      || !strcmp(code, "por") // Portuguese
      || !strcmp(code, "rus") // Russian
      || !strcmp(code, "spa") // Spanish
      || !strcmp(code, "swe") // Swedish
    ;
}

/** \brief Map a string to a ISO 639-2 language code

   Searches the array of ISO 639 codes. If \c s is a valid ISO 639-2
   code, a valid ISO 639-1 code, a valid terminology abbreviation
   for an ISO 639-2 code or the English name for an ISO 639-2 code
   then it returns the index of that entry in the \c iso639_languages array.

   \param c The string to look for in the array of ISO 639 codes.
   \return The index into the \c iso639_languages array if found or
     \c -1 if no such entry was found.
*/
int
map_to_iso639_2_code(const char *s,
                     bool allow_short_english_name) {
  size_t i;
  std::vector<std::string> names;

  for (i = 0; NULL != iso639_languages[i].iso639_2_code; ++i)
    if (                                                        !strcmp(iso639_languages[i].iso639_2_code,      s)
        || ((NULL != iso639_languages[i].terminology_abbrev) && !strcmp(iso639_languages[i].terminology_abbrev, s))
        || ((NULL != iso639_languages[i].iso639_1_code)      && !strcmp(iso639_languages[i].iso639_1_code,      s)))
      return i;

  for (i = 0; NULL != iso639_languages[i].iso639_2_code; ++i) {
    names = split(iso639_languages[i].english_name, ";");
    strip(names);
    size_t j;
    for (j = 0; names.size() > j; ++j)
      if (!strcasecmp(s, names[j].c_str()))
        return i;
  }

  if (!allow_short_english_name)
    return -1;

  size_t len = strlen(s);
  for (i = 0; NULL != iso639_languages[i].iso639_2_code; ++i) {
    names = split(iso639_languages[i].english_name, ";");
    strip(names);
    size_t j;
    for (j = 0; names.size() > j; ++j)
      if (!strncasecmp(s, names[j].c_str(), len))
        return i;
  }

  return -1;
}

