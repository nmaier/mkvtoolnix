  // Write some dummy tags.
  KaxTags &tags = GetChild<KaxTags>(*kax_segment);
  KaxTag &tag = GetChild<KaxTag>(tags);
  KaxTagTargets &targets = GetChild<KaxTagTargets>(tag);
  *(static_cast<EbmlUInteger *>(&GetChild<KaxTagTrackUID>(targets))) =
    1234;
  *(static_cast<EbmlUInteger *>(&GetChild<KaxTagChapterUID>(targets))) =
    5678;
  KaxTagGeneral &general = GetChild<KaxTagGeneral>(tag);
  *(static_cast<EbmlUnicodeString *>(&GetChild<KaxTagSubject>(general))) =
    cstr_to_UTFstring("Subject");
  *(static_cast<EbmlUnicodeString *>(&GetChild<KaxTagBibliography>(general))) =
    cstr_to_UTFstring("Bibliography");
  *(static_cast<EbmlString *>(&GetChild<KaxTagLanguage>(general))) = "ger";
  tags.Render(*out);

    // Also index our tags.
    kax_seekhead->IndexThis(tags, *kax_segment);

