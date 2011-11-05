/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   command line options dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/statline.h>

#include "mmg/cli_options_dlg.h"

std::vector<cli_option_t> cli_options_dlg::all_cli_options;

cli_options_dlg::cli_options_dlg(wxWindow *parent)
  : wxDialog(parent, 0, Z("Add command line options"), wxDefaultPosition, wxSize(400, 350))
{
  if (all_cli_options.empty())
    init_cli_option_list();

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticText(this, -1, Z("Here you can add more command line options either by\n"
                                            "entering them below or by chosing one from the drop\n"
                                            "down box and pressing the 'add' button.")), 0, wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticText(this, -1, Z("Available options:")), 0, wxLEFT, 10);
  siz_all->AddSpacer(5);

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  cob_option           = new wxMTX_COMBOBOX_TYPE(this, ID_CLIOPTIONS_COB, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

  size_t i;
  for (i = 0; i < all_cli_options.size(); i++)
    cob_option->Append(all_cli_options[i].option);
  cob_option->SetSelection(0);
  siz_line->Add(cob_option, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxRIGHT, 15);
  siz_line->Add(new wxButton(this, ID_CLIOPTIONS_ADD, Z("Add")), 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_all->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticText(this, -1, Z("Description:")), 0, wxLEFT, 10);
  siz_all->AddSpacer(5);
  tc_description = new wxTextCtrl(this, -1, all_cli_options[0].description, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
  siz_all->Add(tc_description, 1, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticText(this, -1, Z("Command line options:")), 0, wxLEFT, 10);
  siz_all->AddSpacer(5);
  tc_options = new wxTextCtrl(this, -1);
  siz_all->Add(tc_options, 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);
  siz_all->Add(new wxStaticLine(this, -1), 0, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->AddSpacer(10);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  siz_line->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, 0, 0);
  siz_line->AddSpacer(10);

  siz_all->Add(siz_line, 0, wxGROW, 0);
  siz_all->AddSpacer(10);

  SetSizer(siz_all);
}

void
cli_options_dlg::clear_cli_option_list() {
  all_cli_options.clear();
}

void
cli_options_dlg::init_cli_option_list() {
  clear_cli_option_list();
  all_cli_options.push_back(cli_option_t(  Z("### Global output control ###"),
                                           Z("Several options that control the overall output that mkvmerge creates.")));
  all_cli_options.push_back(cli_option_t(  Z("--cluster-length REPLACEME"),
                                           Z("This option needs an additional argument 'n'. Tells mkvmerge to put at most 'n' data blocks into each cluster. "
                                             "If the number is postfixed with 'ms' then put at most 'n' milliseconds of data into each cluster. "
                                             "The maximum length for a cluster that mkvmerge accepts is 60000 blocks and 32000ms; the minimum length is 100ms. "
                                             "Programs will only be able to seek to clusters, so creating larger clusters may lead to imprecise or slow seeking.")));

  all_cli_options.push_back(cli_option_t(wxU("--no-cues"),
                                           Z("Tells mkvmerge not to create and write the cue data which can be compared to an index in an AVI. "
                                             "Matroska files can be played back without the cue data, but seeking will probably be imprecise and slower. "
                                             "Use this only for testing purposes.")));
  all_cli_options.push_back(cli_option_t(wxU("--clusters-in-meta-seek"),
                                           Z("Tells mkvmerge to create a meta seek element at the end of the file containing all clusters.")));
  all_cli_options.push_back(cli_option_t(wxU("--disable-lacing"),
                                           Z("Disables lacing for all tracks. This will increase the file's size, especially if there are many audio tracks. Use only for testing.")));
  all_cli_options.push_back(cli_option_t(wxU("--enable-durations"),
                                           Z("Write durations for all blocks. This will increase file size and does not offer any additional value for players at the moment.")));
  all_cli_options.push_back(cli_option_t(  Z("--timecode-scale REPLACEME"),
                                           Z("Forces the timecode scale factor to REPLACEME. You have to replace REPLACEME with a value between 1000 and 10000000 or with -1. "
                                             "Normally mkvmerge will use a value of 1000000 which means that timecodes and durations will have a precision of 1ms. "
                                             "For files that will not contain a video track but at least one audio track mkvmerge will automatically choose a timecode scale factor "
                                             "so that all timecodes and durations have a precision of one sample. "
                                             "This causes bigger overhead but allows precise seeking and extraction. "
                                             "If the magical value -1 is used then mkvmerge will use sample precision even if a video track is present.")));
  all_cli_options.push_back(cli_option_t(  Z("### Development hacks ###"),
                                           Z("Options meant ONLY for developpers. Do not use them. If something is considered to be an officially supported option "
                                             "then it's NOT in this list!")));
  all_cli_options.push_back(cli_option_t(wxU("--engage space_after_chapters"),
                                           Z("Leave additional space (EbmlVoid) in the output file after the chapters.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage no_chapters_in_meta_seek"),
                                           Z("Do not add an entry for the chapters in the meta seek element.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage no_meta_seek"),
                                           Z("Do not write meta seek elements at all.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage lacing_xiph"),
                                           Z("Force Xiph style lacing.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage lacing_ebml"),
                                           Z("Force EBML style lacing.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage native_mpeg4"),
                                           Z("Analyze MPEG4 bitstreams, put each frame into one Matroska block, use proper timestamping (I P B B = 0 120 40 80), "
                                             "use V_MPEG4/ISO/... CodecIDs.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage no_variable_data"),
                                           Z("Use fixed values for the elements that change with each file otherwise (muxing date, segment UID, track UIDs etc.). "
                                             "Two files muxed with the same settings and this switch activated will be identical.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage force_passthrough_packetizer"),
                                           Z("Forces the Matroska reader to use the generic passthrough packetizer even for known and supported track types.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage allow_avc_in_vfw_mode"),
                                           Z("Allows storing AVC/h.264 video in Video-for-Windows compatibility mode, e.g. when it is read from an AVI")));
  all_cli_options.push_back(cli_option_t(wxU("--engage no_simpleblocks"),
                                           Z("Disable the use of SimpleBlocks instead of BlockGroups.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage old_aac_codecid"),
                                           Z("Use the old AAC codec IDs (e.g. 'A_AAC/MPEG4/SBR') instead of the new one ('A_AAC').")));
  all_cli_options.push_back(cli_option_t(wxU("--engage use_codec_state"),
                                           Z("Allows the use of the CodecState element. This is used for e.g. MPEG-1/-2 video tracks for storing the sequence headers.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage remove_bitstream_ar_info"),
                                           Z("Normally mkvmerge keeps aspect ratio information in MPEG4 video bitstreams and puts the information into the container. "
                                             "This option causes mkvmerge to remove the aspect ratio information from the bitstream.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage merge_truehd_frames"),
                                           Z("TrueHD audio streams know two frame types: sync frames and non-sync frames. With this switch mkvmerge will put one sync frame "
                                             "and all following non-sync frames into a single Matroska block. Without it each non-sync frame is put into its own Matroska block.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage vobsub_subpic_stop_cmds"),
                                           Z("Causes mkvmerge to add 'stop display' commands to VobSub subtitle packets that do not contain a duration field.")));
  all_cli_options.push_back(cli_option_t(wxU("--engage cow"),
                                           Z("No help available.")));
}

void
cli_options_dlg::on_option_changed(wxCommandEvent &evt) {
  int i = cob_option->GetSelection();
  if (i >= 0)
    tc_description->SetValue(all_cli_options[i].description);
}

void
cli_options_dlg::on_add_clicked(wxCommandEvent &evt) {
  wxString opt = cob_option->GetStringSelection();
  if (opt.Left(3) == wxT("###"))
    return;

  wxString sel = tc_options->GetValue();
  if (sel.length() > 0)
    sel += wxT(" ");
  sel += opt;
  tc_options->SetValue(sel);
}

bool
cli_options_dlg::go(wxString &options) {
  tc_options->SetValue(options);

  if (ShowModal() != wxID_OK)
    return false;

  options = tc_options->GetValue();
  return true;
}

IMPLEMENT_CLASS(cli_options_dlg, wxDialog);
BEGIN_EVENT_TABLE(cli_options_dlg, wxDialog)
  EVT_COMBOBOX(ID_CLIOPTIONS_COB, cli_options_dlg::on_option_changed)
  EVT_BUTTON(ID_CLIOPTIONS_ADD,   cli_options_dlg::on_add_clicked)
END_EVENT_TABLE();

