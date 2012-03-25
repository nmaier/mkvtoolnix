/**
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __COMMON_PROPERTY_TABLE_H
#define __COMMON_PROPERTY_TABLE_H

#include "common/os.h"

#include <map>
#include <string>
#include <vector>

#include <ebml/EbmlElement.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

#include "common/common_pch.h"
#include "common/ebml.h"
#include "common/translation.h"
#include "common/xml/element_mapping.h"

#define NO_CONTAINER EbmlId(static_cast<uint32_t>(0), 0)

class property_element_c {
public:
  std::string m_name;
  translatable_string_c m_title, m_description;

  const EbmlCallbacks *m_callbacks;
  const EbmlCallbacks *m_sub_master_callbacks;

  ebml_type_e m_type;

  property_element_c();
  property_element_c(const std::string &name, const EbmlCallbacks &callbacks, const translatable_string_c &title, const translatable_string_c &description,
                     const EbmlCallbacks &sub_master_callbacks);

  bool is_valid() const;

private:
  void derive_type();

private:                        // static
  static std::map<uint32_t, std::vector<property_element_c> > s_properties;
  static std::map<uint32_t, std::vector<property_element_c> > s_composed_properties;

public:                         // static
  static void init_tables();
  static std::vector<property_element_c> &get_table_for(const EbmlCallbacks &master_callbacks, const EbmlCallbacks *sub_master_callbacks = nullptr, bool full_table = false);
};
typedef std::shared_ptr<property_element_c> property_element_cptr;

#endif  // __COMMON_PROPERTY_TABLE_H
