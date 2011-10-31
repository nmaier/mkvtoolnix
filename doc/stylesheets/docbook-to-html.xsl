<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
 <xsl:output method="html"
             indent="yes"
             doctype-public="-//W3C//DTD HTML 4.01//EN"
             doctype-system="http://www.w3.org/TR/html4/strict.dtd"/>
 <!-- <xsl:strip-space elements="*"/> -->

 <xsl:param name="this-man" select="/refentry/refmeta/refentrytitle"/>

 <xsl:include href="head.xsl"/>
 <xsl:include href="toc.xsl"/>
 <xsl:include href="cmdsynopsis.xsl"/>

 <xsl:template name="generate-anchor">
  <xsl:value-of select="if (not(@id)) then generate-id(title) else @id"/>
 </xsl:template>

 <!-- Enable this for marking elements that match the default rules -->
 <xsl:template match="*">
  <xsl:text>(DEFAULT[</xsl:text><xsl:value-of select="name()"/><xsl:text>]::</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>::DEFAULT)</xsl:text>
 </xsl:template>

 <xsl:template match="/">
  <xsl:apply-templates select="refentry"/>
 </xsl:template>

 <xsl:template match="title"/>

 <xsl:template match="abbrev | classname | code | command | constant | filename | foreignphrase | function | literal | option | parameter | varname">
  <span class="{name()}"><xsl:apply-templates/></span>
 </xsl:template>

 <xsl:template match="productname">
  <span class="productname"><xsl:apply-templates/></span>
  <sup><xsl:text>(tm)</xsl:text></sup>
 </xsl:template>

 <xsl:template match="important">
  <div class="important">
   <p class="important-title">Important:</p>
   <xsl:apply-templates/>
  </div>
 </xsl:template>

 <xsl:template match="note">
  <div class="note">
   <p class="note-title">Note:</p>
   <xsl:apply-templates/>
  </div>
 </xsl:template>

 <xsl:template match="programlisting | screen">
  <pre class="{name()}"><xsl:apply-templates mode="no-markup"/></pre>
 </xsl:template>

 <xsl:template match="refentry">
  <html>
   <xsl:call-template name="head"/>
   <body>
    <div class="heading">
     <xsl:apply-templates select="refnamediv/refname"/>
     <xsl:text> -- </xsl:text>
     <xsl:apply-templates select="refnamediv/refpurpose"/>
    </div>
    <hr/>
    <xsl:call-template name="toc"/>
    <xsl:call-template name="content"/>
   </body>
  </html>
 </xsl:template>

 <xsl:template name="content">
  <div class="content">
   <xsl:for-each select="refsynopsisdiv | refsect1">
    <xsl:if test="name() = 'refsetc1'">
     <hr/>
    </xsl:if>
    <h1>
     <xsl:variable name="anchor">
      <xsl:call-template name="generate-anchor"/>
     </xsl:variable>
     <a name="{$anchor}">
      <xsl:number format="1. " value="position()"/>
      <xsl:value-of select="title"/>
     </a>
    </h1>

    <xsl:apply-templates/>
   </xsl:for-each>
  </div>
 </xsl:template>

 <xsl:template match="refsect2">
  <h2>
   <xsl:variable name="anchor">
    <xsl:call-template name="generate-anchor"/>
   </xsl:variable>
   <a name="{$anchor}">
    <xsl:number level="multiple" format="1. " count="refsynopsisdiv|refsect1|refsect2"/>
    <xsl:value-of select="title"/>
   </a>
  </h2>

  <xsl:apply-templates/>
 </xsl:template>

 <xsl:template match="emphasis">
  <em><xsl:apply-templates/></em>
 </xsl:template>

 <xsl:template match="para">
  <p><xsl:apply-templates/></p>
 </xsl:template>

 <xsl:template match="link">
  <a href="#{@linkend}"><xsl:value-of select="."/></a>
 </xsl:template>

 <xsl:template match="ulink">
  <a href="{@url}"><xsl:value-of select="."/></a>
 </xsl:template>

 <xsl:template match="uri">
  <a href="{.}"><xsl:value-of select="."/></a>
 </xsl:template>

 <xsl:template match="variablelist">
  <table class="variablelist">
   <thead>
    <tr>
     <th>Option</th>
     <th>Description</th>
    </tr>
   </thead>

   <tbody>
    <xsl:apply-templates select="varlistentry"/>
   </tbody>
  </table>
 </xsl:template>

 <xsl:template match="varlistentry">
  <tr>
   <td class="varlistoption">
    <xsl:choose>
     <xsl:when test="@id"><a name="{@id}"><xsl:apply-templates select="term"/></a></xsl:when>
     <xsl:otherwise><xsl:apply-templates select="term"/></xsl:otherwise>
    </xsl:choose>
   </td>
   <td><xsl:apply-templates select="listitem"/></td>
  </tr>
 </xsl:template>

 <xsl:template match="refname | refpurpose | term | varlistentry/listitem">
  <xsl:apply-templates/>
 </xsl:template>

 <xsl:template match="orderedlist">
  <ol>
   <xsl:copy-of select="@id"/>
   <xsl:apply-templates/>
  </ol>
 </xsl:template>

 <xsl:template match="itemizedlist">
  <ul>
   <xsl:copy-of select="@id"/>
   <xsl:apply-templates/>
  </ul>
 </xsl:template>

 <xsl:template match="orderedlist/listitem | itemizedlist/listitem">
  <li>
   <xsl:apply-templates/>
  </li>
 </xsl:template>

 <xsl:template match="optional">
  <xsl:text>[</xsl:text><xsl:apply-templates/><xsl:text>]</xsl:text>
 </xsl:template>

 <xsl:template match="citerefentry">
  <xsl:choose>
   <xsl:when test="$this-man = refentrytitle">
    <span class="productname"><xsl:value-of select="refentrytitle"/></span>
    <xsl:text>(</xsl:text><xsl:value-of select="manvolnum"/><xsl:text>)</xsl:text>
   </xsl:when>
   <xsl:when test="matches(refentrytitle, '^(mmg|mkv)')">
    <a href="{refentrytitle}.html">
     <span class="productname"><xsl:value-of select="refentrytitle"/></span>
     <xsl:text>(</xsl:text><xsl:value-of select="manvolnum"/><xsl:text>)</xsl:text>
    </a>
   </xsl:when>
   <xsl:otherwise>
    <a href="http://linux.die.net/man/{manvolnum}/{refentrytitle}" >
     <span class="productname"><xsl:value-of select="refentrytitle"/></span>
     <xsl:text>(</xsl:text><xsl:value-of select="manvolnum"/><xsl:text>)</xsl:text>
    </a>
   </xsl:otherwise>
  </xsl:choose>
 </xsl:template>
</xsl:stylesheet>
