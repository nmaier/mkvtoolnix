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

#include <unordered_map>

#include "common/iso639.h"
#include "common/strings/editing.h"
#include "common/strings/utf8.h"

std::vector<iso639_language_t> const iso639_languages{
  { "Abkhazian",                                                                        "abk", "ab",          std::string{}  },
  { "Achinese",                                                                         "ace", std::string{}, std::string{}  },
  { "Acoli",                                                                            "ach", std::string{}, std::string{}  },
  { "Adangme",                                                                          "ada", std::string{}, std::string{}  },
  { "Adyghe; Adygei",                                                                   "ady", std::string{}, std::string{}  },
  { "Afar",                                                                             "aar", "aa",          std::string{}  },
  { "Afrihili",                                                                         "afh", std::string{}, std::string{}  },
  { "Afrikaans",                                                                        "afr", "af",          std::string{}  },
  { "Afro-Asiatic languages",                                                           "afa", std::string{}, std::string{}  },
  { "Ainu",                                                                             "ain", std::string{}, std::string{}  },
  { "Akan",                                                                             "aka", "ak",          std::string{}  },
  { "Akkadian",                                                                         "akk", std::string{}, std::string{}  },
  { "Albanian",                                                                         "alb", "sq",          "sqi"          },
  { "Aleut",                                                                            "ale", std::string{}, std::string{}  },
  { "Algonquian languages",                                                             "alg", std::string{}, std::string{}  },
  { "Altaic languages",                                                                 "tut", std::string{}, std::string{}  },
  { "Amharic",                                                                          "amh", "am",          std::string{}  },
  { "Angika",                                                                           "anp", std::string{}, std::string{}  },
  { "Apache languages",                                                                 "apa", std::string{}, std::string{}  },
  { "Arabic",                                                                           "ara", "ar",          std::string{}  },
  { "Aragonese",                                                                        "arg", "an",          std::string{}  },
  { "Arapaho",                                                                          "arp", std::string{}, std::string{}  },
  { "Arawak",                                                                           "arw", std::string{}, std::string{}  },
  { "Armenian",                                                                         "arm", "hy",          "hye"          },
  { "Aromanian; Arumanian; Macedo-Romanian",                                            "rup", std::string{}, std::string{}  },
  { "Artificial languages",                                                             "art", std::string{}, std::string{}  },
  { "Assamese",                                                                         "asm", "as",          std::string{}  },
  { "Asturian; Bable; Leonese; Asturleonese",                                           "ast", std::string{}, std::string{}  },
  { "Athapascan languages",                                                             "ath", std::string{}, std::string{}  },
  { "Australian languages",                                                             "aus", std::string{}, std::string{}  },
  { "Austronesian languages",                                                           "map", std::string{}, std::string{}  },
  { "Avaric",                                                                           "ava", "av",          std::string{}  },
  { "Avestan",                                                                          "ave", "ae",          std::string{}  },
  { "Awadhi",                                                                           "awa", std::string{}, std::string{}  },
  { "Aymara",                                                                           "aym", "ay",          std::string{}  },
  { "Azerbaijani",                                                                      "aze", "az",          std::string{}  },
  { "Balinese",                                                                         "ban", std::string{}, std::string{}  },
  { "Baltic languages",                                                                 "bat", std::string{}, std::string{}  },
  { "Baluchi",                                                                          "bal", std::string{}, std::string{}  },
  { "Bambara",                                                                          "bam", "bm",          std::string{}  },
  { "Bamileke languages",                                                               "bai", std::string{}, std::string{}  },
  { "Banda languages",                                                                  "bad", std::string{}, std::string{}  },
  { "Bantu (Other)",                                                                    "bnt", std::string{}, std::string{}  },
  { "Basa",                                                                             "bas", std::string{}, std::string{}  },
  { "Bashkir",                                                                          "bak", "ba",          std::string{}  },
  { "Basque",                                                                           "baq", "eu",          "eus"          },
  { "Batak languages",                                                                  "btk", std::string{}, std::string{}  },
  { "Beja; Bedawiyet",                                                                  "bej", std::string{}, std::string{}  },
  { "Belarusian",                                                                       "bel", "be",          std::string{}  },
  { "Bemba",                                                                            "bem", std::string{}, std::string{}  },
  { "Bengali",                                                                          "ben", "bn",          std::string{}  },
  { "Berber languages",                                                                 "ber", std::string{}, std::string{}  },
  { "Bhojpuri",                                                                         "bho", std::string{}, std::string{}  },
  { "Bihari languages",                                                                 "bih", "bh",          std::string{}  },
  { "Bikol",                                                                            "bik", std::string{}, std::string{}  },
  { "Bini; Edo",                                                                        "bin", std::string{}, std::string{}  },
  { "Bislama",                                                                          "bis", "bi",          std::string{}  },
  { "Blin; Bilin",                                                                      "byn", std::string{}, std::string{}  },
  { "Blissymbols; Blissymbolics; Bliss",                                                "zbl", std::string{}, std::string{}  },
  { "Bokmål, Norwegian; Norwegian Bokmål",                                              "nob", "nb",          std::string{}  },
  { "Bosnian",                                                                          "bos", "bs",          std::string{}  },
  { "Braj",                                                                             "bra", std::string{}, std::string{}  },
  { "Breton",                                                                           "bre", "br",          std::string{}  },
  { "Buginese",                                                                         "bug", std::string{}, std::string{}  },
  { "Bulgarian",                                                                        "bul", "bg",          std::string{}  },
  { "Buriat",                                                                           "bua", std::string{}, std::string{}  },
  { "Burmese",                                                                          "bur", "my",          "mya"          },
  { "Caddo",                                                                            "cad", std::string{}, std::string{}  },
  { "Catalan; Valencian",                                                               "cat", "ca",          std::string{}  },
  { "Caucasian languages",                                                              "cau", std::string{}, std::string{}  },
  { "Cebuano",                                                                          "ceb", std::string{}, std::string{}  },
  { "Celtic languages",                                                                 "cel", std::string{}, std::string{}  },
  { "Central American Indian languages",                                                "cai", std::string{}, std::string{}  },
  { "Central Khmer",                                                                    "khm", "km",          std::string{}  },
  { "Chagatai",                                                                         "chg", std::string{}, std::string{}  },
  { "Chamic languages",                                                                 "cmc", std::string{}, std::string{}  },
  { "Chamorro",                                                                         "cha", "ch",          std::string{}  },
  { "Chechen",                                                                          "che", "ce",          std::string{}  },
  { "Cherokee",                                                                         "chr", std::string{}, std::string{}  },
  { "Cheyenne",                                                                         "chy", std::string{}, std::string{}  },
  { "Chibcha",                                                                          "chb", std::string{}, std::string{}  },
  { "Chichewa; Chewa; Nyanja",                                                          "nya", "ny",          std::string{}  },
  { "Chinese",                                                                          "chi", "zh",          "zho"          },
  { "Chinook jargon",                                                                   "chn", std::string{}, std::string{}  },
  { "Chipewyan; Dene Suline",                                                           "chp", std::string{}, std::string{}  },
  { "Choctaw",                                                                          "cho", std::string{}, std::string{}  },
  { "Church Slavic; Old Slavonic; Church Slavonic; Old Bulgarian; Old Church Slavonic", "chu", "cu",          std::string{}  },
  { "Chuukese",                                                                         "chk", std::string{}, std::string{}  },
  { "Chuvash",                                                                          "chv", "cv",          std::string{}  },
  { "Classical Newari; Old Newari; Classical Nepal Bhasa",                              "nwc", std::string{}, std::string{}  },
  { "Classical Syriac",                                                                 "syc", std::string{}, std::string{}  },
  { "Coptic",                                                                           "cop", std::string{}, std::string{}  },
  { "Cornish",                                                                          "cor", "kw",          std::string{}  },
  { "Corsican",                                                                         "cos", "co",          std::string{}  },
  { "Cree",                                                                             "cre", "cr",          std::string{}  },
  { "Creek",                                                                            "mus", std::string{}, std::string{}  },
  { "Creoles and pidgins",                                                              "crp", std::string{}, std::string{}  },
  { "Creoles and pidgins, English based",                                               "cpe", std::string{}, std::string{}  },
  { "Creoles and pidgins, French-based",                                                "cpf", std::string{}, std::string{}  },
  { "Creoles and pidgins, Portuguese-based",                                            "cpp", std::string{}, std::string{}  },
  { "Crimean Tatar; Crimean Turkish",                                                   "crh", std::string{}, std::string{}  },
  { "Croatian",                                                                         "hrv", "hr",          std::string{}  },
  { "Cushitic languages",                                                               "cus", std::string{}, std::string{}  },
  { "Czech",                                                                            "cze", "cs",          "ces"          },
  { "Dakota",                                                                           "dak", std::string{}, std::string{}  },
  { "Danish",                                                                           "dan", "da",          std::string{}  },
  { "Dargwa",                                                                           "dar", std::string{}, std::string{}  },
  { "Delaware",                                                                         "del", std::string{}, std::string{}  },
  { "Dinka",                                                                            "din", std::string{}, std::string{}  },
  { "Divehi; Dhivehi; Maldivian",                                                       "div", "dv",          std::string{}  },
  { "Dogri",                                                                            "doi", std::string{}, std::string{}  },
  { "Dogrib",                                                                           "dgr", std::string{}, std::string{}  },
  { "Dravidian languages",                                                              "dra", std::string{}, std::string{}  },
  { "Duala",                                                                            "dua", std::string{}, std::string{}  },
  { "Dutch, Middle (ca.1050-1350)",                                                     "dum", std::string{}, std::string{}  },
  { "Dutch; Flemish",                                                                   "dut", "nl",          "nld"          },
  { "Dyula",                                                                            "dyu", std::string{}, std::string{}  },
  { "Dzongkha",                                                                         "dzo", "dz",          std::string{}  },
  { "Eastern Frisian",                                                                  "frs", std::string{}, std::string{}  },
  { "Efik",                                                                             "efi", std::string{}, std::string{}  },
  { "Egyptian (Ancient)",                                                               "egy", std::string{}, std::string{}  },
  { "Ekajuk",                                                                           "eka", std::string{}, std::string{}  },
  { "Elamite",                                                                          "elx", std::string{}, std::string{}  },
  { "English",                                                                          "eng", "en",          std::string{}  },
  { "English, Middle (1100-1500)",                                                      "enm", std::string{}, std::string{}  },
  { "English, Old (ca.450-1100)",                                                       "ang", std::string{}, std::string{}  },
  { "Erzya",                                                                            "myv", std::string{}, std::string{}  },
  { "Esperanto",                                                                        "epo", "eo",          std::string{}  },
  { "Estonian",                                                                         "est", "et",          std::string{}  },
  { "Ewe",                                                                              "ewe", "ee",          std::string{}  },
  { "Ewondo",                                                                           "ewo", std::string{}, std::string{}  },
  { "Fang",                                                                             "fan", std::string{}, std::string{}  },
  { "Fanti",                                                                            "fat", std::string{}, std::string{}  },
  { "Faroese",                                                                          "fao", "fo",          std::string{}  },
  { "Fijian",                                                                           "fij", "fj",          std::string{}  },
  { "Filipino; Pilipino",                                                               "fil", std::string{}, std::string{}  },
  { "Finnish",                                                                          "fin", "fi",          std::string{}  },
  { "Finno-Ugrian languages",                                                           "fiu", std::string{}, std::string{}  },
  { "Fon",                                                                              "fon", std::string{}, std::string{}  },
  { "French",                                                                           "fre", "fr",          "fra"          },
  { "French, Middle (ca.1400-1600)",                                                    "frm", std::string{}, std::string{}  },
  { "French, Old (842-ca.1400)",                                                        "fro", std::string{}, std::string{}  },
  { "Friulian",                                                                         "fur", std::string{}, std::string{}  },
  { "Fulah",                                                                            "ful", "ff",          std::string{}  },
  { "Ga",                                                                               "gaa", std::string{}, std::string{}  },
  { "Gaelic; Scottish Gaelic",                                                          "gla", "gd",          std::string{}  },
  { "Galibi Carib",                                                                     "car", std::string{}, std::string{}  },
  { "Galician",                                                                         "glg", "gl",          std::string{}  },
  { "Ganda",                                                                            "lug", "lg",          std::string{}  },
  { "Gayo",                                                                             "gay", std::string{}, std::string{}  },
  { "Gbaya",                                                                            "gba", std::string{}, std::string{}  },
  { "Geez",                                                                             "gez", std::string{}, std::string{}  },
  { "Georgian",                                                                         "geo", "ka",          "kat"          },
  { "German",                                                                           "ger", "de",          "deu"          },
  { "German, Middle High (ca.1050-1500)",                                               "gmh", std::string{}, std::string{}  },
  { "German, Old High (ca.750-1050)",                                                   "goh", std::string{}, std::string{}  },
  { "Germanic languages",                                                               "gem", std::string{}, std::string{}  },
  { "Gilbertese",                                                                       "gil", std::string{}, std::string{}  },
  { "Gondi",                                                                            "gon", std::string{}, std::string{}  },
  { "Gorontalo",                                                                        "gor", std::string{}, std::string{}  },
  { "Gothic",                                                                           "got", std::string{}, std::string{}  },
  { "Grebo",                                                                            "grb", std::string{}, std::string{}  },
  { "Greek, Ancient (to 1453)",                                                         "grc", std::string{}, std::string{}  },
  { "Greek, Modern (1453-)",                                                            "gre", "el",          "ell"          },
  { "Guarani",                                                                          "grn", "gn",          std::string{}  },
  { "Gujarati",                                                                         "guj", "gu",          std::string{}  },
  { "Gwich'in",                                                                         "gwi", std::string{}, std::string{}  },
  { "Haida",                                                                            "hai", std::string{}, std::string{}  },
  { "Haitian; Haitian Creole",                                                          "hat", "ht",          std::string{}  },
  { "Hausa",                                                                            "hau", "ha",          std::string{}  },
  { "Hawaiian",                                                                         "haw", std::string{}, std::string{}  },
  { "Hebrew",                                                                           "heb", "he",          std::string{}  },
  { "Herero",                                                                           "her", "hz",          std::string{}  },
  { "Hiligaynon",                                                                       "hil", std::string{}, std::string{}  },
  { "Himachali languages; Western Pahari languages",                                    "him", std::string{}, std::string{}  },
  { "Hindi",                                                                            "hin", "hi",          std::string{}  },
  { "Hiri Motu",                                                                        "hmo", "ho",          std::string{}  },
  { "Hittite",                                                                          "hit", std::string{}, std::string{}  },
  { "Hmong; Mong",                                                                      "hmn", std::string{}, std::string{}  },
  { "Hungarian",                                                                        "hun", "hu",          std::string{}  },
  { "Hupa",                                                                             "hup", std::string{}, std::string{}  },
  { "Iban",                                                                             "iba", std::string{}, std::string{}  },
  { "Icelandic",                                                                        "ice", "is",          "isl"          },
  { "Ido",                                                                              "ido", "io",          std::string{}  },
  { "Igbo",                                                                             "ibo", "ig",          std::string{}  },
  { "Ijo languages",                                                                    "ijo", std::string{}, std::string{}  },
  { "Iloko",                                                                            "ilo", std::string{}, std::string{}  },
  { "Inari Sami",                                                                       "smn", std::string{}, std::string{}  },
  { "Indic languages",                                                                  "inc", std::string{}, std::string{}  },
  { "Indo-European languages",                                                          "ine", std::string{}, std::string{}  },
  { "Indonesian",                                                                       "ind", "id",          std::string{}  },
  { "Ingush",                                                                           "inh", std::string{}, std::string{}  },
  { "Interlingua (International Auxiliary Language Association)",                       "ina", "ia",          std::string{}  },
  { "Interlingue; Occidental",                                                          "ile", "ie",          std::string{}  },
  { "Inuktitut",                                                                        "iku", "iu",          std::string{}  },
  { "Inupiaq",                                                                          "ipk", "ik",          std::string{}  },
  { "Iranian languages",                                                                "ira", std::string{}, std::string{}  },
  { "Irish",                                                                            "gle", "ga",          std::string{}  },
  { "Irish, Middle (900-1200)",                                                         "mga", std::string{}, std::string{}  },
  { "Irish, Old (to 900)",                                                              "sga", std::string{}, std::string{}  },
  { "Iroquoian languages",                                                              "iro", std::string{}, std::string{}  },
  { "Italian",                                                                          "ita", "it",          std::string{}  },
  { "Japanese",                                                                         "jpn", "ja",          std::string{}  },
  { "Javanese",                                                                         "jav", "jv",          std::string{}  },
  { "Judeo-Arabic",                                                                     "jrb", std::string{}, std::string{}  },
  { "Judeo-Persian",                                                                    "jpr", std::string{}, std::string{}  },
  { "Kabardian",                                                                        "kbd", std::string{}, std::string{}  },
  { "Kabyle",                                                                           "kab", std::string{}, std::string{}  },
  { "Kachin; Jingpho",                                                                  "kac", std::string{}, std::string{}  },
  { "Kalaallisut; Greenlandic",                                                         "kal", "kl",          std::string{}  },
  { "Kalmyk; Oirat",                                                                    "xal", std::string{}, std::string{}  },
  { "Kamba",                                                                            "kam", std::string{}, std::string{}  },
  { "Kannada",                                                                          "kan", "kn",          std::string{}  },
  { "Kanuri",                                                                           "kau", "kr",          std::string{}  },
  { "Kara-Kalpak",                                                                      "kaa", std::string{}, std::string{}  },
  { "Karachay-Balkar",                                                                  "krc", std::string{}, std::string{}  },
  { "Karelian",                                                                         "krl", std::string{}, std::string{}  },
  { "Karen languages",                                                                  "kar", std::string{}, std::string{}  },
  { "Kashmiri",                                                                         "kas", "ks",          std::string{}  },
  { "Kashubian",                                                                        "csb", std::string{}, std::string{}  },
  { "Kawi",                                                                             "kaw", std::string{}, std::string{}  },
  { "Kazakh",                                                                           "kaz", "kk",          std::string{}  },
  { "Khasi",                                                                            "kha", std::string{}, std::string{}  },
  { "Khoisan languages",                                                                "khi", std::string{}, std::string{}  },
  { "Khotanese; Sakan",                                                                 "kho", std::string{}, std::string{}  },
  { "Kikuyu; Gikuyu",                                                                   "kik", "ki",          std::string{}  },
  { "Kimbundu",                                                                         "kmb", std::string{}, std::string{}  },
  { "Kinyarwanda",                                                                      "kin", "rw",          std::string{}  },
  { "Kirghiz; Kyrgyz",                                                                  "kir", "ky",          std::string{}  },
  { "Klingon; tlhIngan-Hol",                                                            "tlh", std::string{}, std::string{}  },
  { "Komi",                                                                             "kom", "kv",          std::string{}  },
  { "Kongo",                                                                            "kon", "kg",          std::string{}  },
  { "Konkani",                                                                          "kok", std::string{}, std::string{}  },
  { "Korean",                                                                           "kor", "ko",          std::string{}  },
  { "Kosraean",                                                                         "kos", std::string{}, std::string{}  },
  { "Kpelle",                                                                           "kpe", std::string{}, std::string{}  },
  { "Kru languages",                                                                    "kro", std::string{}, std::string{}  },
  { "Kuanyama; Kwanyama",                                                               "kua", "kj",          std::string{}  },
  { "Kumyk",                                                                            "kum", std::string{}, std::string{}  },
  { "Kurdish",                                                                          "kur", "ku",          std::string{}  },
  { "Kurukh",                                                                           "kru", std::string{}, std::string{}  },
  { "Kutenai",                                                                          "kut", std::string{}, std::string{}  },
  { "Ladino",                                                                           "lad", std::string{}, std::string{}  },
  { "Lahnda",                                                                           "lah", std::string{}, std::string{}  },
  { "Lamba",                                                                            "lam", std::string{}, std::string{}  },
  { "Land Dayak languages",                                                             "day", std::string{}, std::string{}  },
  { "Lao",                                                                              "lao", "lo",          std::string{}  },
  { "Latin",                                                                            "lat", "la",          std::string{}  },
  { "Latvian",                                                                          "lav", "lv",          std::string{}  },
  { "Lezghian",                                                                         "lez", std::string{}, std::string{}  },
  { "Limburgan; Limburger; Limburgish",                                                 "lim", "li",          std::string{}  },
  { "Lingala",                                                                          "lin", "ln",          std::string{}  },
  { "Lithuanian",                                                                       "lit", "lt",          std::string{}  },
  { "Lojban",                                                                           "jbo", std::string{}, std::string{}  },
  { "Low German; Low Saxon; German, Low; Saxon, Low",                                   "nds", std::string{}, std::string{}  },
  { "Lower Sorbian",                                                                    "dsb", std::string{}, std::string{}  },
  { "Lozi",                                                                             "loz", std::string{}, std::string{}  },
  { "Luba-Katanga",                                                                     "lub", "lu",          std::string{}  },
  { "Luba-Lulua",                                                                       "lua", std::string{}, std::string{}  },
  { "Luiseno",                                                                          "lui", std::string{}, std::string{}  },
  { "Lule Sami",                                                                        "smj", std::string{}, std::string{}  },
  { "Lunda",                                                                            "lun", std::string{}, std::string{}  },
  { "Luo (Kenya and Tanzania)",                                                         "luo", std::string{}, std::string{}  },
  { "Lushai",                                                                           "lus", std::string{}, std::string{}  },
  { "Luxembourgish; Letzeburgesch",                                                     "ltz", "lb",          std::string{}  },
  { "Macedonian",                                                                       "mac", "mk",          "mkd"          },
  { "Madurese",                                                                         "mad", std::string{}, std::string{}  },
  { "Magahi",                                                                           "mag", std::string{}, std::string{}  },
  { "Maithili",                                                                         "mai", std::string{}, std::string{}  },
  { "Makasar",                                                                          "mak", std::string{}, std::string{}  },
  { "Malagasy",                                                                         "mlg", "mg",          std::string{}  },
  { "Malay",                                                                            "may", "ms",          "msa"          },
  { "Malayalam",                                                                        "mal", "ml",          std::string{}  },
  { "Maltese",                                                                          "mlt", "mt",          std::string{}  },
  { "Manchu",                                                                           "mnc", std::string{}, std::string{}  },
  { "Mandar",                                                                           "mdr", std::string{}, std::string{}  },
  { "Mandingo",                                                                         "man", std::string{}, std::string{}  },
  { "Manipuri",                                                                         "mni", std::string{}, std::string{}  },
  { "Manobo languages",                                                                 "mno", std::string{}, std::string{}  },
  { "Manx",                                                                             "glv", "gv",          std::string{}  },
  { "Maori",                                                                            "mao", "mi",          "mri"          },
  { "Mapudungun; Mapuche",                                                              "arn", std::string{}, std::string{}  },
  { "Marathi",                                                                          "mar", "mr",          std::string{}  },
  { "Mari",                                                                             "chm", std::string{}, std::string{}  },
  { "Marshallese",                                                                      "mah", "mh",          std::string{}  },
  { "Marwari",                                                                          "mwr", std::string{}, std::string{}  },
  { "Masai",                                                                            "mas", std::string{}, std::string{}  },
  { "Mayan languages",                                                                  "myn", std::string{}, std::string{}  },
  { "Mende",                                                                            "men", std::string{}, std::string{}  },
  { "Mi'kmaq; Micmac",                                                                  "mic", std::string{}, std::string{}  },
  { "Minangkabau",                                                                      "min", std::string{}, std::string{}  },
  { "Mirandese",                                                                        "mwl", std::string{}, std::string{}  },
  { "Mohawk",                                                                           "moh", std::string{}, std::string{}  },
  { "Moksha",                                                                           "mdf", std::string{}, std::string{}  },
  { "Mon-Khmer languages",                                                              "mkh", std::string{}, std::string{}  },
  { "Mongo",                                                                            "lol", std::string{}, std::string{}  },
  { "Mongolian",                                                                        "mon", "mn",          std::string{}  },
  { "Mossi",                                                                            "mos", std::string{}, std::string{}  },
  { "Multiple languages",                                                               "mul", std::string{}, std::string{}  },
  { "Munda languages",                                                                  "mun", std::string{}, std::string{}  },
  { "N'Ko",                                                                             "nqo", std::string{}, std::string{}  },
  { "Nahuatl languages",                                                                "nah", std::string{}, std::string{}  },
  { "Nauru",                                                                            "nau", "na",          std::string{}  },
  { "Navajo; Navaho",                                                                   "nav", "nv",          std::string{}  },
  { "Ndebele, North; North Ndebele",                                                    "nde", "nd",          std::string{}  },
  { "Ndebele, South; South Ndebele",                                                    "nbl", "nr",          std::string{}  },
  { "Ndonga",                                                                           "ndo", "ng",          std::string{}  },
  { "Neapolitan",                                                                       "nap", std::string{}, std::string{}  },
  { "Nepal Bhasa; Newari",                                                              "new", std::string{}, std::string{}  },
  { "Nepali",                                                                           "nep", "ne",          std::string{}  },
  { "Nias",                                                                             "nia", std::string{}, std::string{}  },
  { "Niger-Kordofanian languages",                                                      "nic", std::string{}, std::string{}  },
  { "Nilo-Saharan languages",                                                           "ssa", std::string{}, std::string{}  },
  { "Niuean",                                                                           "niu", std::string{}, std::string{}  },
  { "No linguistic content; Not applicable",                                            "zxx", std::string{}, std::string{}  },
  { "Nogai",                                                                            "nog", std::string{}, std::string{}  },
  { "Norse, Old",                                                                       "non", std::string{}, std::string{}  },
  { "North American Indian languages",                                                  "nai", std::string{}, std::string{}  },
  { "Northern Frisian",                                                                 "frr", std::string{}, std::string{}  },
  { "Northern Sami",                                                                    "sme", "se",          std::string{}  },
  { "Norwegian Nynorsk; Nynorsk, Norwegian",                                            "nno", "nn",          std::string{}  },
  { "Norwegian",                                                                        "nor", "no",          std::string{}  },
  { "Nubian languages",                                                                 "nub", std::string{}, std::string{}  },
  { "Nyamwezi",                                                                         "nym", std::string{}, std::string{}  },
  { "Nyankole",                                                                         "nyn", std::string{}, std::string{}  },
  { "Nyoro",                                                                            "nyo", std::string{}, std::string{}  },
  { "Nzima",                                                                            "nzi", std::string{}, std::string{}  },
  { "Occitan (post 1500)",                                                              "oci", "oc",          std::string{}  },
  { "Official Aramaic (700-300 BCE); Imperial Aramaic (700-300 BCE)",                   "arc", std::string{}, std::string{}  },
  { "Ojibwa",                                                                           "oji", "oj",          std::string{}  },
  { "Oriya",                                                                            "ori", "or",          std::string{}  },
  { "Oromo",                                                                            "orm", "om",          std::string{}  },
  { "Osage",                                                                            "osa", std::string{}, std::string{}  },
  { "Ossetian; Ossetic",                                                                "oss", "os",          std::string{}  },
  { "Otomian languages",                                                                "oto", std::string{}, std::string{}  },
  { "Pahlavi",                                                                          "pal", std::string{}, std::string{}  },
  { "Palauan",                                                                          "pau", std::string{}, std::string{}  },
  { "Pali",                                                                             "pli", "pi",          std::string{}  },
  { "Pampanga; Kapampangan",                                                            "pam", std::string{}, std::string{}  },
  { "Pangasinan",                                                                       "pag", std::string{}, std::string{}  },
  { "Panjabi; Punjabi",                                                                 "pan", "pa",          std::string{}  },
  { "Papiamento",                                                                       "pap", std::string{}, std::string{}  },
  { "Papuan languages",                                                                 "paa", std::string{}, std::string{}  },
  { "Pedi; Sepedi; Northern Sotho",                                                     "nso", std::string{}, std::string{}  },
  { "Persian",                                                                          "per", "fa",          "fas"          },
  { "Persian, Old (ca.600-400 B.C.)",                                                   "peo", std::string{}, std::string{}  },
  { "Philippine languages",                                                             "phi", std::string{}, std::string{}  },
  { "Phoenician",                                                                       "phn", std::string{}, std::string{}  },
  { "Pohnpeian",                                                                        "pon", std::string{}, std::string{}  },
  { "Polish",                                                                           "pol", "pl",          std::string{}  },
  { "Portuguese",                                                                       "por", "pt",          std::string{}  },
  { "Prakrit languages",                                                                "pra", std::string{}, std::string{}  },
  { "Provençal, Old (to 1500);Occitan, Old (to 1500)",                                  "pro", std::string{}, std::string{}  },
  { "Pushto; Pashto",                                                                   "pus", "ps",          std::string{}  },
  { "Quechua",                                                                          "que", "qu",          std::string{}  },
  { "Rajasthani",                                                                       "raj", std::string{}, std::string{}  },
  { "Rapanui",                                                                          "rap", std::string{}, std::string{}  },
  { "Rarotongan; Cook Islands Maori",                                                   "rar", std::string{}, std::string{}  },
  { "Romance languages",                                                                "roa", std::string{}, std::string{}  },
  { "Romanian; Moldavian; Moldovan",                                                    "rum", "ro",          "ron"          },
  { "Romansh",                                                                          "roh", "rm",          std::string{}  },
  { "Romany",                                                                           "rom", std::string{}, std::string{}  },
  { "Rundi",                                                                            "run", "rn",          std::string{}  },
  { "Russian",                                                                          "rus", "ru",          std::string{}  },
  { "Salishan languages",                                                               "sal", std::string{}, std::string{}  },
  { "Samaritan Aramaic",                                                                "sam", std::string{}, std::string{}  },
  { "Sami languages",                                                                   "smi", std::string{}, std::string{}  },
  { "Samoan",                                                                           "smo", "sm",          std::string{}  },
  { "Sandawe",                                                                          "sad", std::string{}, std::string{}  },
  { "Sango",                                                                            "sag", "sg",          std::string{}  },
  { "Sanskrit",                                                                         "san", "sa",          std::string{}  },
  { "Santali",                                                                          "sat", std::string{}, std::string{}  },
  { "Sardinian",                                                                        "srd", "sc",          std::string{}  },
  { "Sasak",                                                                            "sas", std::string{}, std::string{}  },
  { "Scots",                                                                            "sco", std::string{}, std::string{}  },
  { "Selkup",                                                                           "sel", std::string{}, std::string{}  },
  { "Semitic languages",                                                                "sem", std::string{}, std::string{}  },
  { "Serbian",                                                                          "srp", "sr",          std::string{}  },
  { "Serer",                                                                            "srr", std::string{}, std::string{}  },
  { "Shan",                                                                             "shn", std::string{}, std::string{}  },
  { "Shona",                                                                            "sna", "sn",          std::string{}  },
  { "Sichuan Yi; Nuosu",                                                                "iii", "ii",          std::string{}  },
  { "Sicilian",                                                                         "scn", std::string{}, std::string{}  },
  { "Sidamo",                                                                           "sid", std::string{}, std::string{}  },
  { "Sign Languages",                                                                   "sgn", std::string{}, std::string{}  },
  { "Siksika",                                                                          "bla", std::string{}, std::string{}  },
  { "Sindhi",                                                                           "snd", "sd",          std::string{}  },
  { "Sinhala; Sinhalese",                                                               "sin", "si",          std::string{}  },
  { "Sino-Tibetan languages",                                                           "sit", std::string{}, std::string{}  },
  { "Siouan languages",                                                                 "sio", std::string{}, std::string{}  },
  { "Skolt Sami",                                                                       "sms", std::string{}, std::string{}  },
  { "Slave (Athapascan)",                                                               "den", std::string{}, std::string{}  },
  { "Slavic languages",                                                                 "sla", std::string{}, std::string{}  },
  { "Slovak",                                                                           "slo", "sk",          "slk"          },
  { "Slovenian",                                                                        "slv", "sl",          std::string{}  },
  { "Sogdian",                                                                          "sog", std::string{}, std::string{}  },
  { "Somali",                                                                           "som", "so",          std::string{}  },
  { "Songhai languages",                                                                "son", std::string{}, std::string{}  },
  { "Soninke",                                                                          "snk", std::string{}, std::string{}  },
  { "Sorbian languages",                                                                "wen", std::string{}, std::string{}  },
  { "Sotho, Southern",                                                                  "sot", "st",          std::string{}  },
  { "South American Indian (Other)",                                                    "sai", std::string{}, std::string{}  },
  { "Southern Altai",                                                                   "alt", std::string{}, std::string{}  },
  { "Southern Sami",                                                                    "sma", std::string{}, std::string{}  },
  { "Spanish; Castillan",                                                               "spa", "es",          std::string{}  },
  { "Sranan Tongo",                                                                     "srn", std::string{}, std::string{}  },
  { "Sukuma",                                                                           "suk", std::string{}, std::string{}  },
  { "Sumerian",                                                                         "sux", std::string{}, std::string{}  },
  { "Sundanese",                                                                        "sun", "su",          std::string{}  },
  { "Susu",                                                                             "sus", std::string{}, std::string{}  },
  { "Swahili",                                                                          "swa", "sw",          std::string{}  },
  { "Swati",                                                                            "ssw", "ss",          std::string{}  },
  { "Swedish",                                                                          "swe", "sv",          std::string{}  },
  { "Swiss German; Alemannic; Alsatian",                                                "gsw", std::string{}, std::string{}  },
  { "Syriac",                                                                           "syr", std::string{}, std::string{}  },
  { "Tagalog",                                                                          "tgl", "tl",          std::string{}  },
  { "Tahitian",                                                                         "tah", "ty",          std::string{}  },
  { "Tai languages",                                                                    "tai", std::string{}, std::string{}  },
  { "Tajik",                                                                            "tgk", "tg",          std::string{}  },
  { "Tamashek",                                                                         "tmh", std::string{}, std::string{}  },
  { "Tamil",                                                                            "tam", "ta",          std::string{}  },
  { "Tatar",                                                                            "tat", "tt",          std::string{}  },
  { "Telugu",                                                                           "tel", "te",          std::string{}  },
  { "Tereno",                                                                           "ter", std::string{}, std::string{}  },
  { "Tetum",                                                                            "tet", std::string{}, std::string{}  },
  { "Thai",                                                                             "tha", "th",          std::string{}  },
  { "Tibetan",                                                                          "tib", "bo",          "bod"          },
  { "Tigre",                                                                            "tig", std::string{}, std::string{}  },
  { "Tigrinya",                                                                         "tir", "ti",          std::string{}  },
  { "Timne",                                                                            "tem", std::string{}, std::string{}  },
  { "Tiv",                                                                              "tiv", std::string{}, std::string{}  },
  { "Tlingit",                                                                          "tli", std::string{}, std::string{}  },
  { "Tok Pisin",                                                                        "tpi", std::string{}, std::string{}  },
  { "Tokelau",                                                                          "tkl", std::string{}, std::string{}  },
  { "Tonga (Nyasa)",                                                                    "tog", std::string{}, std::string{}  },
  { "Tonga (Tonga Islands)",                                                            "ton", "to",          std::string{}  },
  { "Tsimshian",                                                                        "tsi", std::string{}, std::string{}  },
  { "Tsonga",                                                                           "tso", "ts",          std::string{}  },
  { "Tswana",                                                                           "tsn", "tn",          std::string{}  },
  { "Tumbuka",                                                                          "tum", std::string{}, std::string{}  },
  { "Tupi languages",                                                                   "tup", std::string{}, std::string{}  },
  { "Turkish",                                                                          "tur", "tr",          std::string{}  },
  { "Turkish, Ottoman (1500-1928)",                                                     "ota", std::string{}, std::string{}  },
  { "Turkmen",                                                                          "tuk", "tk",          std::string{}  },
  { "Tuvalu",                                                                           "tvl", std::string{}, std::string{}  },
  { "Tuvinian",                                                                         "tyv", std::string{}, std::string{}  },
  { "Twi",                                                                              "twi", "tw",          std::string{}  },
  { "Udmurt",                                                                           "udm", std::string{}, std::string{}  },
  { "Ugaritic",                                                                         "uga", std::string{}, std::string{}  },
  { "Uighur; Uyghur",                                                                   "uig", "ug",          std::string{}  },
  { "Ukrainian",                                                                        "ukr", "uk",          std::string{}  },
  { "Umbundu",                                                                          "umb", std::string{}, std::string{}  },
  { "Uncoded languages",                                                                "mis", std::string{}, std::string{}  },
  { "Undetermined",                                                                     "und", std::string{}, std::string{}  },
  { "Upper Sorbian",                                                                    "hsb", std::string{}, std::string{}  },
  { "Urdu",                                                                             "urd", "ur",          std::string{}  },
  { "Uzbek",                                                                            "uzb", "uz",          std::string{}  },
  { "Vai",                                                                              "vai", std::string{}, std::string{}  },
  { "Venda",                                                                            "ven", "ve",          std::string{}  },
  { "Vietnamese",                                                                       "vie", "vi",          std::string{}  },
  { "Volapük",                                                                          "vol", "vo",          std::string{}  },
  { "Votic",                                                                            "vot", std::string{}, std::string{}  },
  { "Wakashan languages",                                                               "wak", std::string{}, std::string{}  },
  { "Walamo",                                                                           "wal", std::string{}, std::string{}  },
  { "Walloon",                                                                          "wln", "wa",          std::string{}  },
  { "Waray",                                                                            "war", std::string{}, std::string{}  },
  { "Washo",                                                                            "was", std::string{}, std::string{}  },
  { "Welsh",                                                                            "wel", "cy",          "cym"          },
  { "Western Frisian",                                                                  "fry", "fy",          std::string{}  },
  { "Wolof",                                                                            "wol", "wo",          std::string{}  },
  { "Xhosa",                                                                            "xho", "xh",          std::string{}  },
  { "Yakut",                                                                            "sah", std::string{}, std::string{}  },
  { "Yao",                                                                              "yao", std::string{}, std::string{}  },
  { "Yapese",                                                                           "yap", std::string{}, std::string{}  },
  { "Yiddish",                                                                          "yid", "yi",          std::string{}  },
  { "Yoruba",                                                                           "yor", "yo",          std::string{}  },
  { "Yupik languages",                                                                  "ypk", std::string{}, std::string{}  },
  { "Zande languages",                                                                  "znd", std::string{}, std::string{}  },
  { "Zapotec",                                                                          "zap", std::string{}, std::string{}  },
  { "Zaza; Dimili; Dimli; Kirdki; Kirmanjki; Zazaki",                                   "zza", std::string{}, std::string{}  },
  { "Zenaga",                                                                           "zen", std::string{}, std::string{}  },
  { "Zhuang; Chuang",                                                                   "zha", "za",          std::string{}  },
  { "Zulu",                                                                             "zul", "zu",          std::string{}  },
  { "Zuni",                                                                             "zun", std::string{}, std::string{}  },
};

std::unordered_map<std::string, std::string> s_deprecated_1_and_2_codes{
  // ISO 639-1
  { "iw", "he" },

  // ISO 639-2
  { "scr", "hrv" },
  { "scc", "srp" },
  { "mol", "rum" },
};

bool
is_valid_iso639_2_code(std::string const &iso639_2_code) {
  return brng::find_if(iso639_languages, [&](iso639_language_t const &lang) { return lang.iso639_2_code == iso639_2_code; }) != iso639_languages.end();
}

#define FILL(s, idx) s + std::wstring(longest[idx] - get_width_in_em(s), L' ')

void
list_iso639_languages() {
  std::wstring w_col1 = to_wide(Y("English language name"));
  std::wstring w_col2 = to_wide(Y("ISO639-2 code"));
  std::wstring w_col3 = to_wide(Y("ISO639-1 code"));

  size_t longest[3]   = { get_width_in_em(w_col1), get_width_in_em(w_col2), get_width_in_em(w_col3) };

  for (auto &lang : iso639_languages) {
    longest[0] = std::max(longest[0], get_width_in_em(to_wide(lang.english_name)));
    longest[1] = std::max(longest[1], get_width_in_em(to_wide(lang.iso639_2_code)));
    longest[2] = std::max(longest[2], get_width_in_em(to_wide(lang.iso639_1_code)));
  }

  mxinfo(FILL(w_col1, 0) + L" | " + FILL(w_col2, 1) + L" | " + FILL(w_col3, 2) + L"\n");
  mxinfo(std::wstring(longest[0] + 1, L'-') + L'+' + std::wstring(longest[1] + 2, L'-') + L'+' + std::wstring(longest[2] + 1, L'-') + L"\n");

  for (auto &lang : iso639_languages) {
    std::wstring english = to_wide(lang.english_name);
    std::wstring code2   = to_wide(lang.iso639_2_code);
    std::wstring code1   = to_wide(lang.iso639_1_code);
    mxinfo(FILL(english, 0) + L" | " + FILL(code2, 1) + L" | " + FILL(code1, 2) + L"\n");
  }
}

std::string const &
map_iso639_2_to_iso639_1(std::string const &iso639_2_code) {
  auto lang = brng::find_if(iso639_languages, [&](iso639_language_t const &lang) { return lang.iso639_2_code == iso639_2_code; });
  return (lang != iso639_languages.end()) ? lang->iso639_1_code : empty_string;
}

bool
is_popular_language(std::string const &language) {
  static std::vector<std::string> s_popular_languages = { "Chinese", "Dutch", "English", "Finnish", "French", "German", "Italian", "Japanese", "Norwegian", "Portuguese", "Russian", "Spanish", "Spanish; Castillan", "Swedish" };
  return brng::find(s_popular_languages, language) != s_popular_languages.end();
}

bool
is_popular_language_code(std::string const &code) {
  static std::vector<std::string> s_popular_language_codes = { "chi", "dut", "eng", "fin", "fre", "ger", "ita", "jpn", "nor", "por", "rus", "spa", "swe" };
  return brng::find(s_popular_language_codes, code) != s_popular_language_codes.end();
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
map_to_iso639_2_code(std::string const &s,
                     bool allow_short_english_name) {
  auto source          = s;
  auto deprecated_code = s_deprecated_1_and_2_codes.find(source);
  if (deprecated_code != s_deprecated_1_and_2_codes.end())
    source = deprecated_code->second;

  auto lang = brng::find_if(iso639_languages, [&](iso639_language_t const &lang) { return (lang.iso639_2_code == source) || (lang.terminology_abbrev == source) || (lang.iso639_1_code == source); });
  if (lang != iso639_languages.end())
    return std::distance(iso639_languages.begin(), lang);

  auto range = iso639_languages | badap::indexed(0);
  auto end   = boost::end(range);
  for (auto lang = boost::begin(range); lang != end; lang++) {
    auto names = split(lang->english_name, ";");
    strip(names);
    if (brng::find(names, source) != names.end())
      return lang.index();
  }

  if (!allow_short_english_name)
    return -1;

  for (auto lang = boost::begin(range); lang != end; lang++) {
    auto names = split(lang->english_name, ";");
    strip(names);
    if (names.end() != brng::find_if(names, [&](std::string const &name) { return balg::istarts_with(name, source); }))
      return lang.index();
  }

  return -1;
}
