<?xml version="1.0" encoding="utf-8"?>

<!--

Chapters to cue sheet
=====================

This is a XSLT 2.0 stylesheet for turning the chapter format that mkvmerge
understands into a cue sheet as used by e.g. CD recording software.

You need a XSLT 2.0 compatible XSLT processor, e.g. Saxon-HE (see
http://saxon.sourceforge.net/).

Usage depends on your processor. For Saxon-HE on Linux this should work:

$ java -classpath saxon9he.jar net.sf.saxon.Transform \
    -o:output-filename.cue \
    -xsl:chapters-to-cuesheet.xsl \
    input-filename.xml

You have to extract the chapters from a Matroska file with mkvextract first if
you want to turn the chapters included in one into a cue sheet.

This can be useful if you want to turn an audio file contain in a Matroska
file into one file per chapter (e.g. for music videos). For this you need both
"chapters-to-cuesheet.xsl" and "chapters-to-shnsplit.xsl" as well as the
"cuetools" (see https://github.com/svend/cuetools) and "shntool" (see
http://etree.org/shnutils/shntool/) packages.

Assumptions are (meaning: adjust these values in the commands):

- the source file contains an audio track with track ID 2 of type FLAC,
- the band is "Perpetuum Jazzile",
- the album is "Live 2009",
- the source file name is "source.mkv".

Here we go:

  # 1. Extract audio track from Matroska file:
  $ mkvextract source.mkv tracks 2:source.flac

  # 2. Extract chapters from same file:
  $ mkvextract source.mkv chapters > chapters.xml

  # 3. Generate the split points used in step 4:
  $ java -classpath saxon9he.jar net.sf.saxon.Transform \
      -o:splitpoints.txt \
      -xsl:chapters-to-cuesheet.xsl \
      chapters.xml

  # 4. Split the audio file into multiple files:
  $ shnsplit -o flac source.flac < splitpoints.txt

  # 5. Generate the cue sheet used in step 6:
  $ java -classpath saxon9he.jar net.sf.saxon.Transform \
      -o:source.cue \
      -xsl:chapters-to-cuesheet.xsl \
      chapters.xml \
      artist="Perpetuum Jazzile" \
      album="Live 2009"

  # 6: Assign tags (artist and title):
  $ cuetag source.cue split-*.flac

Written by Moritz Bunkus <moritz@bunkus.org>.

-->

<xsl:stylesheet version="2.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:xs="http://www.w3.org/2001/XMLSchema">
 <xsl:param name="album" select="'ChangeMeAlbum'"/>
 <xsl:param name="artist" select="'ChangeMeAlbumArtist'"/>
 <xsl:param name="track-artist" select="if ($artist eq 'ChangeMeAlbumArtist') then 'ChangeMeTrackArtist' else $artist"/>

 <xsl:output method="text"/>
 <xsl:strip-space elements="*"/>

 <xsl:template match="/Chapters">
  <xsl:apply-templates select="EditionEntry"/>
 </xsl:template>

 <xsl:template name="timecode-to-ns" as="xs:integer">
  <xsl:param name="timecode" select="'0'"/>
  <xsl:choose>
   <xsl:when test="$timecode eq ''">
    <xsl:value-of select="0"/>
   </xsl:when>
   <xsl:when test="matches($timecode, '^\d+:\d+:\d+\.\d{9}$')">
    <xsl:analyze-string select="$timecode"
                        regex="^ (\d+) : (\d+) : (\d+) \. (\d+) $"
                        flags="x">
     <xsl:matching-substring>
      <xsl:value-of select="  (regex-group(1) cast as xs:integer) * 3600000000000
                            + (regex-group(2) cast as xs:integer) *   60000000000
                            + (regex-group(3) cast as xs:integer) *    1000000000
                            + (regex-group(4) cast as xs:integer)"/>
     </xsl:matching-substring>
    </xsl:analyze-string>
   </xsl:when>
   <xsl:when test="matches($timecode, '^\d+:\d+:\d+\.\d*$')">
    <xsl:call-template name="timecode-to-ns">
     <xsl:with-param name="timecode" select="concat($timecode, '0')"/>
    </xsl:call-template>
   </xsl:when>
   <xsl:when test="matches($timecode, '^\d+:\d+:\d+$')">
    <xsl:call-template name="timecode-to-ns">
     <xsl:with-param name="timecode" select="concat($timecode, '.')"/>
    </xsl:call-template>
   </xsl:when>
   <xsl:otherwise>
    <xsl:call-template name="timecode-to-ns">
     <xsl:with-param name="timecode" select="concat('0:', $timecode)"/>
    </xsl:call-template>
   </xsl:otherwise>
  </xsl:choose>
 </xsl:template>

 <xsl:template match="EditionEntry">
  <xsl:text>PERFORMER "</xsl:text>
  <xsl:value-of select="$artist"/>
  <xsl:text>"
TITLE "</xsl:text>
  <xsl:value-of select="$album"/>
  <xsl:text>"
FILE "ChangeMe.mp3" MP3
</xsl:text>
  <xsl:for-each select="ChapterAtom">
   <xsl:variable name="timecode" as="xs:integer">
    <xsl:call-template name="timecode-to-ns">
     <xsl:with-param name="timecode" select="ChapterTimeStart"/>
    </xsl:call-template>
   </xsl:variable>
   <xsl:text>  TRACK </xsl:text>
   <xsl:number format="01" value="position()"/>
   <xsl:text> AUDIO
    PERFORMER "</xsl:text>
   <xsl:value-of select="$track-artist"/>
   <xsl:text>"
    TITLE "</xsl:text>
   <xsl:value-of select="ChapterDisplay[1]/ChapterString"/>
   <xsl:text>"
    INDEX 01 </xsl:text>
    <xsl:number format="01" value=" $timecode idiv 60000000000"/>
    <xsl:text>:</xsl:text>
    <xsl:number format="01" value="($timecode idiv  1000000000) mod 60"/>
    <xsl:text>:</xsl:text>
   <xsl:value-of select="($timecode mod 1000000000) * 75 idiv 1000000000"/>
   <xsl:text>&#xa;</xsl:text>
  </xsl:for-each>
 </xsl:template>
</xsl:stylesheet>
