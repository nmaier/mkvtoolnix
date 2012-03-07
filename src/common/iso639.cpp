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
  { "Abkhazian",                                                                        "abk", "ab", nullptr  },
  { "Achinese",                                                                         "ace", nullptr, nullptr  },
  { "Acoli",                                                                            "ach", nullptr, nullptr  },
  { "Adangme",                                                                          "ada", nullptr, nullptr  },
  { "Adyghe; Adygei",                                                                   "ady", nullptr, nullptr  },
  { "Afar",                                                                             "aar", "aa", nullptr  },
  { "Afrihili",                                                                         "afh", nullptr, nullptr  },
  { "Afrikaans",                                                                        "afr", "af", nullptr  },
  { "Afro-Asiatic languages",                                                           "afa", nullptr, nullptr  },
  { "Ainu",                                                                             "ain", nullptr, nullptr  },
  { "Akan",                                                                             "aka", "ak", nullptr  },
  { "Akkadian",                                                                         "akk", nullptr, nullptr  },
  { "Albanian",                                                                         "alb", "sq", "sqi" },
  { "Aleut",                                                                            "ale", nullptr, nullptr  },
  { "Algonquian languages",                                                             "alg", nullptr, nullptr  },
  { "Altaic languages",                                                                 "tut", nullptr, nullptr  },
  { "Amharic",                                                                          "amh", "am", nullptr  },
  { "Angika",                                                                           "anp", nullptr, nullptr  },
  { "Apache languages",                                                                 "apa", nullptr, nullptr  },
  { "Arabic",                                                                           "ara", "ar", nullptr  },
  { "Aragonese",                                                                        "arg", "an", nullptr  },
  { "Arapaho",                                                                          "arp", nullptr, nullptr  },
  { "Arawak",                                                                           "arw", nullptr, nullptr  },
  { "Armenian",                                                                         "arm", "hy", "hye" },
  { "Aromanian; Arumanian; Macedo-Romanian",                                            "rup", nullptr, nullptr  },
  { "Artificial languages",                                                             "art", nullptr, nullptr  },
  { "Assamese",                                                                         "asm", "as", nullptr  },
  { "Asturian; Bable; Leonese; Asturleonese",                                           "ast", nullptr, nullptr  },
  { "Athapascan languages",                                                             "ath", nullptr, nullptr  },
  { "Australian languages",                                                             "aus", nullptr, nullptr  },
  { "Austronesian languages",                                                           "map", nullptr, nullptr  },
  { "Avaric",                                                                           "ava", "av", nullptr  },
  { "Avestan",                                                                          "ave", "ae", nullptr  },
  { "Awadhi",                                                                           "awa", nullptr, nullptr  },
  { "Aymara",                                                                           "aym", "ay", nullptr  },
  { "Azerbaijani",                                                                      "aze", "az", nullptr  },
  { "Balinese",                                                                         "ban", nullptr, nullptr  },
  { "Baltic languages",                                                                 "bat", nullptr, nullptr  },
  { "Baluchi",                                                                          "bal", nullptr, nullptr  },
  { "Bambara",                                                                          "bam", "bm", nullptr  },
  { "Bamileke languages",                                                               "bai", nullptr, nullptr  },
  { "Banda languages",                                                                  "bad", nullptr, nullptr  },
  { "Bantu (Other)",                                                                    "bnt", nullptr, nullptr  },
  { "Basa",                                                                             "bas", nullptr, nullptr  },
  { "Bashkir",                                                                          "bak", "ba", nullptr  },
  { "Basque",                                                                           "baq", "eu", "eus" },
  { "Batak languages",                                                                  "btk", nullptr, nullptr  },
  { "Beja; Bedawiyet",                                                                  "bej", nullptr, nullptr  },
  { "Belarusian",                                                                       "bel", "be", nullptr  },
  { "Bemba",                                                                            "bem", nullptr, nullptr  },
  { "Bengali",                                                                          "ben", "bn", nullptr  },
  { "Berber languages",                                                                 "ber", nullptr, nullptr  },
  { "Bhojpuri",                                                                         "bho", nullptr, nullptr  },
  { "Bihari languages",                                                                 "bih", "bh", nullptr  },
  { "Bikol",                                                                            "bik", nullptr, nullptr  },
  { "Bini; Edo",                                                                        "bin", nullptr, nullptr  },
  { "Bislama",                                                                          "bis", "bi", nullptr  },
  { "Blin; Bilin",                                                                      "byn", nullptr, nullptr  },
  { "Blissymbols; Blissymbolics; Bliss",                                                "zbl", nullptr, nullptr  },
  { "Bokmål, Norwegian; Norwegian Bokmål",                                              "nob", "nb", nullptr  },
  { "Bosnian",                                                                          "bos", "bs", nullptr  },
  { "Braj",                                                                             "bra", nullptr, nullptr  },
  { "Breton",                                                                           "bre", "br", nullptr  },
  { "Buginese",                                                                         "bug", nullptr, nullptr  },
  { "Bulgarian",                                                                        "bul", "bg", nullptr  },
  { "Buriat",                                                                           "bua", nullptr, nullptr  },
  { "Burmese",                                                                          "bur", "my", "mya" },
  { "Caddo",                                                                            "cad", nullptr, nullptr  },
  { "Catalan; Valencian",                                                               "cat", "ca", nullptr  },
  { "Caucasian languages",                                                              "cau", nullptr, nullptr  },
  { "Cebuano",                                                                          "ceb", nullptr, nullptr  },
  { "Celtic languages",                                                                 "cel", nullptr, nullptr  },
  { "Central American Indian languages",                                                "cai", nullptr, nullptr  },
  { "Central Khmer",                                                                    "khm", "km", nullptr  },
  { "Chagatai",                                                                         "chg", nullptr, nullptr  },
  { "Chamic languages",                                                                 "cmc", nullptr, nullptr  },
  { "Chamorro",                                                                         "cha", "ch", nullptr  },
  { "Chechen",                                                                          "che", "ce", nullptr  },
  { "Cherokee",                                                                         "chr", nullptr, nullptr  },
  { "Cheyenne",                                                                         "chy", nullptr, nullptr  },
  { "Chibcha",                                                                          "chb", nullptr, nullptr  },
  { "Chichewa; Chewa; Nyanja",                                                          "nya", "ny", nullptr  },
  { "Chinese",                                                                          "chi", "zh", "zho" },
  { "Chinook jargon",                                                                   "chn", nullptr, nullptr  },
  { "Chipewyan; Dene Suline",                                                           "chp", nullptr, nullptr  },
  { "Choctaw",                                                                          "cho", nullptr, nullptr  },
  { "Church Slavic; Old Slavonic; Church Slavonic; Old Bulgarian; Old Church Slavonic", "chu", "cu", nullptr  },
  { "Chuukese",                                                                         "chk", nullptr, nullptr  },
  { "Chuvash",                                                                          "chv", "cv", nullptr  },
  { "Classical Newari; Old Newari; Classical Nepal Bhasa",                              "nwc", nullptr, nullptr  },
  { "Classical Syriac",                                                                 "syc", nullptr, nullptr  },
  { "Coptic",                                                                           "cop", nullptr, nullptr  },
  { "Cornish",                                                                          "cor", "kw", nullptr  },
  { "Corsican",                                                                         "cos", "co", nullptr  },
  { "Cree",                                                                             "cre", "cr", nullptr  },
  { "Creek",                                                                            "mus", nullptr, nullptr  },
  { "Creoles and pidgins",                                                              "crp", nullptr, nullptr  },
  { "Creoles and pidgins, English based",                                               "cpe", nullptr, nullptr  },
  { "Creoles and pidgins, French-based",                                                "cpf", nullptr, nullptr  },
  { "Creoles and pidgins, Portuguese-based",                                            "cpp", nullptr, nullptr  },
  { "Crimean Tatar; Crimean Turkish",                                                   "crh", nullptr, nullptr  },
  { "Croatian",                                                                         "hrv", "hr", nullptr  },
  { "Cushitic languages",                                                               "cus", nullptr, nullptr  },
  { "Czech",                                                                            "cze", "cs", "ces" },
  { "Dakota",                                                                           "dak", nullptr, nullptr  },
  { "Danish",                                                                           "dan", "da", nullptr  },
  { "Dargwa",                                                                           "dar", nullptr, nullptr  },
  { "Delaware",                                                                         "del", nullptr, nullptr  },
  { "Dinka",                                                                            "din", nullptr, nullptr  },
  { "Divehi; Dhivehi; Maldivian",                                                       "div", "dv", nullptr  },
  { "Dogri",                                                                            "doi", nullptr, nullptr  },
  { "Dogrib",                                                                           "dgr", nullptr, nullptr  },
  { "Dravidian languages",                                                              "dra", nullptr, nullptr  },
  { "Duala",                                                                            "dua", nullptr, nullptr  },
  { "Dutch, Middle (ca.1050-1350)",                                                     "dum", nullptr, nullptr  },
  { "Dutch; Flemish",                                                                   "dut", "nl", "nld" },
  { "Dyula",                                                                            "dyu", nullptr, nullptr  },
  { "Dzongkha",                                                                         "dzo", "dz", nullptr  },
  { "Eastern Frisian",                                                                  "frs", nullptr, nullptr  },
  { "Efik",                                                                             "efi", nullptr, nullptr  },
  { "Egyptian (Ancient)",                                                               "egy", nullptr, nullptr  },
  { "Ekajuk",                                                                           "eka", nullptr, nullptr  },
  { "Elamite",                                                                          "elx", nullptr, nullptr  },
  { "English",                                                                          "eng", "en", nullptr  },
  { "English, Middle (1100-1500)",                                                      "enm", nullptr, nullptr  },
  { "English, Old (ca.450-1100)",                                                       "ang", nullptr, nullptr  },
  { "Erzya",                                                                            "myv", nullptr, nullptr  },
  { "Esperanto",                                                                        "epo", "eo", nullptr  },
  { "Estonian",                                                                         "est", "et", nullptr  },
  { "Ewe",                                                                              "ewe", "ee", nullptr  },
  { "Ewondo",                                                                           "ewo", nullptr, nullptr  },
  { "Fang",                                                                             "fan", nullptr, nullptr  },
  { "Fanti",                                                                            "fat", nullptr, nullptr  },
  { "Faroese",                                                                          "fao", "fo", nullptr  },
  { "Fijian",                                                                           "fij", "fj", nullptr  },
  { "Filipino; Pilipino",                                                               "fil", nullptr, nullptr  },
  { "Finnish",                                                                          "fin", "fi", nullptr  },
  { "Finno-Ugrian languages",                                                           "fiu", nullptr, nullptr  },
  { "Fon",                                                                              "fon", nullptr, nullptr  },
  { "French",                                                                           "fre", "fr", "fra" },
  { "French, Middle (ca.1400-1600)",                                                    "frm", nullptr, nullptr  },
  { "French, Old (842-ca.1400)",                                                        "fro", nullptr, nullptr  },
  { "Friulian",                                                                         "fur", nullptr, nullptr  },
  { "Fulah",                                                                            "ful", "ff", nullptr  },
  { "Ga",                                                                               "gaa", nullptr, nullptr  },
  { "Gaelic; Scottish Gaelic",                                                          "gla", "gd", nullptr  },
  { "Galibi Carib",                                                                     "car", nullptr, nullptr  },
  { "Galician",                                                                         "glg", "gl", nullptr  },
  { "Ganda",                                                                            "lug", "lg", nullptr  },
  { "Gayo",                                                                             "gay", nullptr, nullptr  },
  { "Gbaya",                                                                            "gba", nullptr, nullptr  },
  { "Geez",                                                                             "gez", nullptr, nullptr  },
  { "Georgian",                                                                         "geo", "ka", "kat" },
  { "German",                                                                           "ger", "de", "deu" },
  { "German, Middle High (ca.1050-1500)",                                               "gmh", nullptr, nullptr  },
  { "German, Old High (ca.750-1050)",                                                   "goh", nullptr, nullptr  },
  { "Germanic languages",                                                               "gem", nullptr, nullptr  },
  { "Gilbertese",                                                                       "gil", nullptr, nullptr  },
  { "Gondi",                                                                            "gon", nullptr, nullptr  },
  { "Gorontalo",                                                                        "gor", nullptr, nullptr  },
  { "Gothic",                                                                           "got", nullptr, nullptr  },
  { "Grebo",                                                                            "grb", nullptr, nullptr  },
  { "Greek, Ancient (to 1453)",                                                         "grc", nullptr, nullptr  },
  { "Greek, Modern (1453-)",                                                            "gre", "el", "ell" },
  { "Guarani",                                                                          "grn", "gn", nullptr  },
  { "Gujarati",                                                                         "guj", "gu", nullptr  },
  { "Gwich'in",                                                                         "gwi", nullptr, nullptr  },
  { "Haida",                                                                            "hai", nullptr, nullptr  },
  { "Haitian; Haitian Creole",                                                          "hat", "ht", nullptr  },
  { "Hausa",                                                                            "hau", "ha", nullptr  },
  { "Hawaiian",                                                                         "haw", nullptr, nullptr  },
  { "Hebrew",                                                                           "heb", "he", nullptr  },
  { "Herero",                                                                           "her", "hz", nullptr  },
  { "Hiligaynon",                                                                       "hil", nullptr, nullptr  },
  { "Himachali languages; Western Pahari languages",                                    "him", nullptr, nullptr  },
  { "Hindi",                                                                            "hin", "hi", nullptr  },
  { "Hiri Motu",                                                                        "hmo", "ho", nullptr  },
  { "Hittite",                                                                          "hit", nullptr, nullptr  },
  { "Hmong; Mong",                                                                      "hmn", nullptr, nullptr  },
  { "Hungarian",                                                                        "hun", "hu", nullptr  },
  { "Hupa",                                                                             "hup", nullptr, nullptr  },
  { "Iban",                                                                             "iba", nullptr, nullptr  },
  { "Icelandic",                                                                        "ice", "is", "isl" },
  { "Ido",                                                                              "ido", "io", nullptr  },
  { "Igbo",                                                                             "ibo", "ig", nullptr  },
  { "Ijo languages",                                                                    "ijo", nullptr, nullptr  },
  { "Iloko",                                                                            "ilo", nullptr, nullptr  },
  { "Inari Sami",                                                                       "smn", nullptr, nullptr  },
  { "Indic languages",                                                                  "inc", nullptr, nullptr  },
  { "Indo-European languages",                                                          "ine", nullptr, nullptr  },
  { "Indonesian",                                                                       "ind", "id", nullptr  },
  { "Ingush",                                                                           "inh", nullptr, nullptr  },
  { "Interlingua (International Auxiliary Language Association)",                       "ina", "ia", nullptr  },
  { "Interlingue; Occidental",                                                          "ile", "ie", nullptr  },
  { "Inuktitut",                                                                        "iku", "iu", nullptr  },
  { "Inupiaq",                                                                          "ipk", "ik", nullptr  },
  { "Iranian languages",                                                                "ira", nullptr, nullptr  },
  { "Irish",                                                                            "gle", "ga", nullptr  },
  { "Irish, Middle (900-1200)",                                                         "mga", nullptr, nullptr  },
  { "Irish, Old (to 900)",                                                              "sga", nullptr, nullptr  },
  { "Iroquoian languages",                                                              "iro", nullptr, nullptr  },
  { "Italian",                                                                          "ita", "it", nullptr  },
  { "Japanese",                                                                         "jpn", "ja", nullptr  },
  { "Javanese",                                                                         "jav", "jv", nullptr  },
  { "Judeo-Arabic",                                                                     "jrb", nullptr, nullptr  },
  { "Judeo-Persian",                                                                    "jpr", nullptr, nullptr  },
  { "Kabardian",                                                                        "kbd", nullptr, nullptr  },
  { "Kabyle",                                                                           "kab", nullptr, nullptr  },
  { "Kachin; Jingpho",                                                                  "kac", nullptr, nullptr  },
  { "Kalaallisut; Greenlandic",                                                         "kal", "kl", nullptr  },
  { "Kalmyk; Oirat",                                                                    "xal", nullptr, nullptr  },
  { "Kamba",                                                                            "kam", nullptr, nullptr  },
  { "Kannada",                                                                          "kan", "kn", nullptr  },
  { "Kanuri",                                                                           "kau", "kr", nullptr  },
  { "Kara-Kalpak",                                                                      "kaa", nullptr, nullptr  },
  { "Karachay-Balkar",                                                                  "krc", nullptr, nullptr  },
  { "Karelian",                                                                         "krl", nullptr, nullptr  },
  { "Karen languages",                                                                  "kar", nullptr, nullptr  },
  { "Kashmiri",                                                                         "kas", "ks", nullptr  },
  { "Kashubian",                                                                        "csb", nullptr, nullptr  },
  { "Kawi",                                                                             "kaw", nullptr, nullptr  },
  { "Kazakh",                                                                           "kaz", "kk", nullptr  },
  { "Khasi",                                                                            "kha", nullptr, nullptr  },
  { "Khoisan languages",                                                                "khi", nullptr, nullptr  },
  { "Khotanese; Sakan",                                                                 "kho", nullptr, nullptr  },
  { "Kikuyu; Gikuyu",                                                                   "kik", "ki", nullptr  },
  { "Kimbundu",                                                                         "kmb", nullptr, nullptr  },
  { "Kinyarwanda",                                                                      "kin", "rw", nullptr  },
  { "Kirghiz; Kyrgyz",                                                                  "kir", "ky", nullptr  },
  { "Klingon; tlhIngan-Hol",                                                            "tlh", nullptr, nullptr  },
  { "Komi",                                                                             "kom", "kv", nullptr  },
  { "Kongo",                                                                            "kon", "kg", nullptr  },
  { "Konkani",                                                                          "kok", nullptr, nullptr  },
  { "Korean",                                                                           "kor", "ko", nullptr  },
  { "Kosraean",                                                                         "kos", nullptr, nullptr  },
  { "Kpelle",                                                                           "kpe", nullptr, nullptr  },
  { "Kru languages",                                                                    "kro", nullptr, nullptr  },
  { "Kuanyama; Kwanyama",                                                               "kua", "kj", nullptr  },
  { "Kumyk",                                                                            "kum", nullptr, nullptr  },
  { "Kurdish",                                                                          "kur", "ku", nullptr  },
  { "Kurukh",                                                                           "kru", nullptr, nullptr  },
  { "Kutenai",                                                                          "kut", nullptr, nullptr  },
  { "Ladino",                                                                           "lad", nullptr, nullptr  },
  { "Lahnda",                                                                           "lah", nullptr, nullptr  },
  { "Lamba",                                                                            "lam", nullptr, nullptr  },
  { "Land Dayak languages",                                                             "day", nullptr, nullptr  },
  { "Lao",                                                                              "lao", "lo", nullptr  },
  { "Latin",                                                                            "lat", "la", nullptr  },
  { "Latvian",                                                                          "lav", "lv", nullptr  },
  { "Lezghian",                                                                         "lez", nullptr, nullptr  },
  { "Limburgan; Limburger; Limburgish",                                                 "lim", "li", nullptr  },
  { "Lingala",                                                                          "lin", "ln", nullptr  },
  { "Lithuanian",                                                                       "lit", "lt", nullptr  },
  { "Lojban",                                                                           "jbo", nullptr, nullptr  },
  { "Low German; Low Saxon; German, Low; Saxon, Low",                                   "nds", nullptr, nullptr  },
  { "Lower Sorbian",                                                                    "dsb", nullptr, nullptr  },
  { "Lozi",                                                                             "loz", nullptr, nullptr  },
  { "Luba-Katanga",                                                                     "lub", "lu", nullptr  },
  { "Luba-Lulua",                                                                       "lua", nullptr, nullptr  },
  { "Luiseno",                                                                          "lui", nullptr, nullptr  },
  { "Lule Sami",                                                                        "smj", nullptr, nullptr  },
  { "Lunda",                                                                            "lun", nullptr, nullptr  },
  { "Luo (Kenya and Tanzania)",                                                         "luo", nullptr, nullptr  },
  { "Lushai",                                                                           "lus", nullptr, nullptr  },
  { "Luxembourgish; Letzeburgesch",                                                     "ltz", "lb", nullptr  },
  { "Macedonian",                                                                       "mac", "mk", "mkd" },
  { "Madurese",                                                                         "mad", nullptr, nullptr  },
  { "Magahi",                                                                           "mag", nullptr, nullptr  },
  { "Maithili",                                                                         "mai", nullptr, nullptr  },
  { "Makasar",                                                                          "mak", nullptr, nullptr  },
  { "Malagasy",                                                                         "mlg", "mg", nullptr  },
  { "Malay",                                                                            "may", "ms", "msa" },
  { "Malayalam",                                                                        "mal", "ml", nullptr  },
  { "Maltese",                                                                          "mlt", "mt", nullptr  },
  { "Manchu",                                                                           "mnc", nullptr, nullptr  },
  { "Mandar",                                                                           "mdr", nullptr, nullptr  },
  { "Mandingo",                                                                         "man", nullptr, nullptr  },
  { "Manipuri",                                                                         "mni", nullptr, nullptr  },
  { "Manobo languages",                                                                 "mno", nullptr, nullptr  },
  { "Manx",                                                                             "glv", "gv", nullptr  },
  { "Maori",                                                                            "mao", "mi", "mri" },
  { "Mapudungun; Mapuche",                                                              "arn", nullptr, nullptr  },
  { "Marathi",                                                                          "mar", "mr", nullptr  },
  { "Mari",                                                                             "chm", nullptr, nullptr  },
  { "Marshallese",                                                                      "mah", "mh", nullptr  },
  { "Marwari",                                                                          "mwr", nullptr, nullptr  },
  { "Masai",                                                                            "mas", nullptr, nullptr  },
  { "Mayan languages",                                                                  "myn", nullptr, nullptr  },
  { "Mende",                                                                            "men", nullptr, nullptr  },
  { "Mi'kmaq; Micmac",                                                                  "mic", nullptr, nullptr  },
  { "Minangkabau",                                                                      "min", nullptr, nullptr  },
  { "Mirandese",                                                                        "mwl", nullptr, nullptr  },
  { "Mohawk",                                                                           "moh", nullptr, nullptr  },
  { "Moksha",                                                                           "mdf", nullptr, nullptr  },
  { "Mon-Khmer languages",                                                              "mkh", nullptr, nullptr  },
  { "Mongo",                                                                            "lol", nullptr, nullptr  },
  { "Mongolian",                                                                        "mon", "mn", nullptr  },
  { "Mossi",                                                                            "mos", nullptr, nullptr  },
  { "Multiple languages",                                                               "mul", nullptr, nullptr  },
  { "Munda languages",                                                                  "mun", nullptr, nullptr  },
  { "N'Ko",                                                                             "nqo", nullptr, nullptr  },
  { "Nahuatl languages",                                                                "nah", nullptr, nullptr  },
  { "Nauru",                                                                            "nau", "na", nullptr  },
  { "Navajo; Navaho",                                                                   "nav", "nv", nullptr  },
  { "Ndebele, North; North Ndebele",                                                    "nde", "nd", nullptr  },
  { "Ndebele, South; South Ndebele",                                                    "nbl", "nr", nullptr  },
  { "Ndonga",                                                                           "ndo", "ng", nullptr  },
  { "Neapolitan",                                                                       "nap", nullptr, nullptr  },
  { "Nepal Bhasa; Newari",                                                              "new", nullptr, nullptr  },
  { "Nepali",                                                                           "nep", "ne", nullptr  },
  { "Nias",                                                                             "nia", nullptr, nullptr  },
  { "Niger-Kordofanian languages",                                                      "nic", nullptr, nullptr  },
  { "Nilo-Saharan languages",                                                           "ssa", nullptr, nullptr  },
  { "Niuean",                                                                           "niu", nullptr, nullptr  },
  { "No linguistic content; Not applicable",                                            "zxx", nullptr, nullptr  },
  { "Nogai",                                                                            "nog", nullptr, nullptr  },
  { "Norse, Old",                                                                       "non", nullptr, nullptr  },
  { "North American Indian languages",                                                  "nai", nullptr, nullptr  },
  { "Northern Frisian",                                                                 "frr", nullptr, nullptr  },
  { "Northern Sami",                                                                    "sme", "se", nullptr  },
  { "Norwegian Nynorsk; Nynorsk, Norwegian",                                            "nno", "nn", nullptr  },
  { "Norwegian",                                                                        "nor", "no", nullptr  },
  { "Nubian languages",                                                                 "nub", nullptr, nullptr  },
  { "Nyamwezi",                                                                         "nym", nullptr, nullptr  },
  { "Nyankole",                                                                         "nyn", nullptr, nullptr  },
  { "Nyoro",                                                                            "nyo", nullptr, nullptr  },
  { "Nzima",                                                                            "nzi", nullptr, nullptr  },
  { "Occitan (post 1500)",                                                              "oci", "oc", nullptr  },
  { "Official Aramaic (700-300 BCE); Imperial Aramaic (700-300 BCE)",                   "arc", nullptr, nullptr  },
  { "Ojibwa",                                                                           "oji", "oj", nullptr  },
  { "Oriya",                                                                            "ori", "or", nullptr  },
  { "Oromo",                                                                            "orm", "om", nullptr  },
  { "Osage",                                                                            "osa", nullptr, nullptr  },
  { "Ossetian; Ossetic",                                                                "oss", "os", nullptr  },
  { "Otomian languages",                                                                "oto", nullptr, nullptr  },
  { "Pahlavi",                                                                          "pal", nullptr, nullptr  },
  { "Palauan",                                                                          "pau", nullptr, nullptr  },
  { "Pali",                                                                             "pli", "pi", nullptr  },
  { "Pampanga; Kapampangan",                                                            "pam", nullptr, nullptr  },
  { "Pangasinan",                                                                       "pag", nullptr, nullptr  },
  { "Panjabi; Punjabi",                                                                 "pan", "pa", nullptr  },
  { "Papiamento",                                                                       "pap", nullptr, nullptr  },
  { "Papuan languages",                                                                 "paa", nullptr, nullptr  },
  { "Pedi; Sepedi; Northern Sotho",                                                     "nso", nullptr, nullptr  },
  { "Persian",                                                                          "per", "fa", "fas" },
  { "Persian, Old (ca.600-400 B.C.)",                                                   "peo", nullptr, nullptr  },
  { "Philippine languages",                                                             "phi", nullptr, nullptr  },
  { "Phoenician",                                                                       "phn", nullptr, nullptr  },
  { "Pohnpeian",                                                                        "pon", nullptr, nullptr  },
  { "Polish",                                                                           "pol", "pl", nullptr  },
  { "Portuguese",                                                                       "por", "pt", nullptr  },
  { "Prakrit languages",                                                                "pra", nullptr, nullptr  },
  { "Provençal, Old (to 1500);Occitan, Old (to 1500)",                                  "pro", nullptr, nullptr  },
  { "Pushto; Pashto",                                                                   "pus", "ps", nullptr  },
  { "Quechua",                                                                          "que", "qu", nullptr  },
  { "Rajasthani",                                                                       "raj", nullptr, nullptr  },
  { "Rapanui",                                                                          "rap", nullptr, nullptr  },
  { "Rarotongan; Cook Islands Maori",                                                   "rar", nullptr, nullptr  },
  { "Romance languages",                                                                "roa", nullptr, nullptr  },
  { "Romanian; Moldavian; Moldovan",                                                    "rum", "ro", "ron" },
  { "Romansh",                                                                          "roh", "rm", nullptr  },
  { "Romany",                                                                           "rom", nullptr, nullptr  },
  { "Rundi",                                                                            "run", "rn", nullptr  },
  { "Russian",                                                                          "rus", "ru", nullptr  },
  { "Salishan languages",                                                               "sal", nullptr, nullptr  },
  { "Samaritan Aramaic",                                                                "sam", nullptr, nullptr  },
  { "Sami languages",                                                                   "smi", nullptr, nullptr  },
  { "Samoan",                                                                           "smo", "sm", nullptr  },
  { "Sandawe",                                                                          "sad", nullptr, nullptr  },
  { "Sango",                                                                            "sag", "sg", nullptr  },
  { "Sanskrit",                                                                         "san", "sa", nullptr  },
  { "Santali",                                                                          "sat", nullptr, nullptr  },
  { "Sardinian",                                                                        "srd", "sc", nullptr  },
  { "Sasak",                                                                            "sas", nullptr, nullptr  },
  { "Scots",                                                                            "sco", nullptr, nullptr  },
  { "Selkup",                                                                           "sel", nullptr, nullptr  },
  { "Semitic languages",                                                                "sem", nullptr, nullptr  },
  { "Serbian",                                                                          "srp", "sr", nullptr  },
  { "Serer",                                                                            "srr", nullptr, nullptr  },
  { "Shan",                                                                             "shn", nullptr, nullptr  },
  { "Shona",                                                                            "sna", "sn", nullptr  },
  { "Sichuan Yi; Nuosu",                                                                "iii", "ii", nullptr  },
  { "Sicilian",                                                                         "scn", nullptr, nullptr  },
  { "Sidamo",                                                                           "sid", nullptr, nullptr  },
  { "Sign Languages",                                                                   "sgn", nullptr, nullptr  },
  { "Siksika",                                                                          "bla", nullptr, nullptr  },
  { "Sindhi",                                                                           "snd", "sd", nullptr  },
  { "Sinhala; Sinhalese",                                                               "sin", "si", nullptr  },
  { "Sino-Tibetan languages",                                                           "sit", nullptr, nullptr  },
  { "Siouan languages",                                                                 "sio", nullptr, nullptr  },
  { "Skolt Sami",                                                                       "sms", nullptr, nullptr  },
  { "Slave (Athapascan)",                                                               "den", nullptr, nullptr  },
  { "Slavic languages",                                                                 "sla", nullptr, nullptr  },
  { "Slovak",                                                                           "slo", "sk", "slk" },
  { "Slovenian",                                                                        "slv", "sl", nullptr  },
  { "Sogdian",                                                                          "sog", nullptr, nullptr  },
  { "Somali",                                                                           "som", "so", nullptr  },
  { "Songhai languages",                                                                "son", nullptr, nullptr  },
  { "Soninke",                                                                          "snk", nullptr, nullptr  },
  { "Sorbian languages",                                                                "wen", nullptr, nullptr  },
  { "Sotho, Southern",                                                                  "sot", "st", nullptr  },
  { "South American Indian (Other)",                                                    "sai", nullptr, nullptr  },
  { "Southern Altai",                                                                   "alt", nullptr, nullptr  },
  { "Southern Sami",                                                                    "sma", nullptr, nullptr  },
  { "Spanish; Castillan",                                                               "spa", "es", nullptr  },
  { "Sranan Tongo",                                                                     "srn", nullptr, nullptr  },
  { "Sukuma",                                                                           "suk", nullptr, nullptr  },
  { "Sumerian",                                                                         "sux", nullptr, nullptr  },
  { "Sundanese",                                                                        "sun", "su", nullptr  },
  { "Susu",                                                                             "sus", nullptr, nullptr  },
  { "Swahili",                                                                          "swa", "sw", nullptr  },
  { "Swati",                                                                            "ssw", "ss", nullptr  },
  { "Swedish",                                                                          "swe", "sv", nullptr  },
  { "Swiss German; Alemannic; Alsatian",                                                "gsw", nullptr, nullptr  },
  { "Syriac",                                                                           "syr", nullptr, nullptr  },
  { "Tagalog",                                                                          "tgl", "tl", nullptr  },
  { "Tahitian",                                                                         "tah", "ty", nullptr  },
  { "Tai languages",                                                                    "tai", nullptr, nullptr  },
  { "Tajik",                                                                            "tgk", "tg", nullptr  },
  { "Tamashek",                                                                         "tmh", nullptr, nullptr  },
  { "Tamil",                                                                            "tam", "ta", nullptr  },
  { "Tatar",                                                                            "tat", "tt", nullptr  },
  { "Telugu",                                                                           "tel", "te", nullptr  },
  { "Tereno",                                                                           "ter", nullptr, nullptr  },
  { "Tetum",                                                                            "tet", nullptr, nullptr  },
  { "Thai",                                                                             "tha", "th", nullptr  },
  { "Tibetan",                                                                          "tib", "bo", "bod" },
  { "Tigre",                                                                            "tig", nullptr, nullptr  },
  { "Tigrinya",                                                                         "tir", "ti", nullptr  },
  { "Timne",                                                                            "tem", nullptr, nullptr  },
  { "Tiv",                                                                              "tiv", nullptr, nullptr  },
  { "Tlingit",                                                                          "tli", nullptr, nullptr  },
  { "Tok Pisin",                                                                        "tpi", nullptr, nullptr  },
  { "Tokelau",                                                                          "tkl", nullptr, nullptr  },
  { "Tonga (Nyasa)",                                                                    "tog", nullptr, nullptr  },
  { "Tonga (Tonga Islands)",                                                            "ton", "to", nullptr  },
  { "Tsimshian",                                                                        "tsi", nullptr, nullptr  },
  { "Tsonga",                                                                           "tso", "ts", nullptr  },
  { "Tswana",                                                                           "tsn", "tn", nullptr  },
  { "Tumbuka",                                                                          "tum", nullptr, nullptr  },
  { "Tupi languages",                                                                   "tup", nullptr, nullptr  },
  { "Turkish",                                                                          "tur", "tr", nullptr  },
  { "Turkish, Ottoman (1500-1928)",                                                     "ota", nullptr, nullptr  },
  { "Turkmen",                                                                          "tuk", "tk", nullptr  },
  { "Tuvalu",                                                                           "tvl", nullptr, nullptr  },
  { "Tuvinian",                                                                         "tyv", nullptr, nullptr  },
  { "Twi",                                                                              "twi", "tw", nullptr  },
  { "Udmurt",                                                                           "udm", nullptr, nullptr  },
  { "Ugaritic",                                                                         "uga", nullptr, nullptr  },
  { "Uighur; Uyghur",                                                                   "uig", "ug", nullptr  },
  { "Ukrainian",                                                                        "ukr", "uk", nullptr  },
  { "Umbundu",                                                                          "umb", nullptr, nullptr  },
  { "Uncoded languages",                                                                "mis", nullptr, nullptr  },
  { "Undetermined",                                                                     "und", nullptr, nullptr  },
  { "Upper Sorbian",                                                                    "hsb", nullptr, nullptr  },
  { "Urdu",                                                                             "urd", "ur", nullptr  },
  { "Uzbek",                                                                            "uzb", "uz", nullptr  },
  { "Vai",                                                                              "vai", nullptr, nullptr  },
  { "Venda",                                                                            "ven", "ve", nullptr  },
  { "Vietnamese",                                                                       "vie", "vi", nullptr  },
  { "Volapük",                                                                          "vol", "vo", nullptr  },
  { "Votic",                                                                            "vot", nullptr, nullptr  },
  { "Wakashan languages",                                                               "wak", nullptr, nullptr  },
  { "Walamo",                                                                           "wal", nullptr, nullptr  },
  { "Walloon",                                                                          "wln", "wa", nullptr  },
  { "Waray",                                                                            "war", nullptr, nullptr  },
  { "Washo",                                                                            "was", nullptr, nullptr  },
  { "Welsh",                                                                            "wel", "cy", "cym" },
  { "Western Frisian",                                                                  "fry", "fy", nullptr  },
  { "Wolof",                                                                            "wol", "wo", nullptr  },
  { "Xhosa",                                                                            "xho", "xh", nullptr  },
  { "Yakut",                                                                            "sah", nullptr, nullptr  },
  { "Yao",                                                                              "yao", nullptr, nullptr  },
  { "Yapese",                                                                           "yap", nullptr, nullptr  },
  { "Yiddish",                                                                          "yid", "yi", nullptr  },
  { "Yoruba",                                                                           "yor", "yo", nullptr  },
  { "Yupik languages",                                                                  "ypk", nullptr, nullptr  },
  { "Zande languages",                                                                  "znd", nullptr, nullptr  },
  { "Zapotec",                                                                          "zap", nullptr, nullptr  },
  { "Zaza; Dimili; Dimli; Kirdki; Kirmanjki; Zazaki",                                   "zza", nullptr, nullptr  },
  { "Zenaga",                                                                           "zen", nullptr, nullptr  },
  { "Zhuang; Chuang",                                                                   "zha", "za", nullptr  },
  { "Zulu",                                                                             "zul", "zu", nullptr  },
  { "Zuni",                                                                             "zun", nullptr, nullptr  },
  { nullptr,                                                                               nullptr,  nullptr, nullptr  },
};

bool
is_valid_iso639_2_code(const char *iso639_2_code) {
  int i = 0;
  while (iso639_languages[i].iso639_2_code != nullptr) {
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

  for (i = 0; nullptr != iso639_languages[i].iso639_2_code; ++i) {
    longest[0] = std::max(longest[0], get_width_in_em(to_wide(iso639_languages[i].english_name)));
    longest[1] = std::max(longest[1], get_width_in_em(to_wide(iso639_languages[i].iso639_2_code)));
    longest[2] = std::max(longest[2], get_width_in_em(to_wide(nullptr != iso639_languages[i].iso639_1_code ? iso639_languages[i].iso639_1_code : "")));
  }

  mxinfo(FILL(w_col1, 0) + L" | " + FILL(w_col2, 1) + L" | " + FILL(w_col3, 2) + L"\n");
  mxinfo(std::wstring(longest[0] + 1, L'-') + L'+' + std::wstring(longest[1] + 2, L'-') + L'+' + std::wstring(longest[2] + 1, L'-') + L"\n");

  for (i = 0; nullptr != iso639_languages[i].iso639_2_code; ++i) {
    std::wstring english = to_wide(iso639_languages[i].english_name);
    std::wstring code2   = to_wide(iso639_languages[i].iso639_2_code);
    std::wstring code1   = nullptr != iso639_languages[i].iso639_1_code ? to_wide(iso639_languages[i].iso639_1_code) : L"";
    mxinfo(FILL(english, 0) + L" | " + FILL(code2, 1) + L" | " + FILL(code1, 2) + L"\n");
  }
}

const char *
map_iso639_2_to_iso639_1(const char *iso639_2_code) {
  uint32_t i;

  for (i = 0; iso639_languages[i].iso639_2_code != nullptr; i++)
    if (!strcmp(iso639_2_code, iso639_languages[i].iso639_2_code))
      return iso639_languages[i].iso639_1_code;

  return nullptr;
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

  for (i = 0; nullptr != iso639_languages[i].iso639_2_code; ++i)
    if (                                                        !strcmp(iso639_languages[i].iso639_2_code,      s)
        || ((nullptr != iso639_languages[i].terminology_abbrev) && !strcmp(iso639_languages[i].terminology_abbrev, s))
        || ((nullptr != iso639_languages[i].iso639_1_code)      && !strcmp(iso639_languages[i].iso639_1_code,      s)))
      return i;

  for (i = 0; nullptr != iso639_languages[i].iso639_2_code; ++i) {
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
  for (i = 0; nullptr != iso639_languages[i].iso639_2_code; ++i) {
    names = split(iso639_languages[i].english_name, ";");
    strip(names);
    size_t j;
    for (j = 0; names.size() > j; ++j)
      if (!strncasecmp(s, names[j].c_str(), len))
        return i;
  }

  return -1;
}

