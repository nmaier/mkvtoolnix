<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
 <xsl:template name="toc">
  <div id="toc" class="toc">
   <h1><xsl:text>Table of contents</xsl:text></h1>

   <ul>
    <xsl:for-each select="refsynopsisdiv | refsect1">
     <li>
      <xsl:variable name="anchor">
       <xsl:call-template name="generate-anchor"/>
      </xsl:variable>

      <a href="#{$anchor}">
       <xsl:number level="multiple" format="1. " count="refsynopsisdiv|refsect1"/>
       <xsl:value-of select="title"/>
      </a>
     </li>
    </xsl:for-each>
   </ul>
  </div>
 </xsl:template>
</xsl:stylesheet>
