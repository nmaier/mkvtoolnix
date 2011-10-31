<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
 <xsl:template name="head">
  <head>
   <title>
    <xsl:value-of select="refnamediv/refname"/>
    <xsl:text> -- </xsl:text>
    <xsl:value-of select="refnamediv/refpurpose"/>
   </title>
   <link rel="stylesheet" type="text/css" href="mkvtoolnix-doc.css"/>
  </head>
 </xsl:template>
</xsl:stylesheet>
