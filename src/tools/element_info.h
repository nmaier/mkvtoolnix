#ifndef __ELEMENT_INFO
#define __ELEMENT_INFO

#include "common/common_pch.h"

extern std::map<uint32_t, std::string> g_element_names;
extern std::map<uint32_t, bool> g_master_information;

void init_element_names();
void init_master_information();
uint32_t element_name_to_id(const std::string &name);

#endif  // __ELEMENT_INFO
