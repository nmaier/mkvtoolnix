<?xml version="1.0" encoding="utf-8"?>

<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
 <xsl:template match="cmdsynopsis">
  <div class="cmdsynopsis">
   <span class="commandname"><xsl:value-of select="command"/></span>
   <xsl:apply-templates select="group | arg">
    <xsl:with-param name="previous" select="'normal'"/>
   </xsl:apply-templates>
  </div>
 </xsl:template>

 <xsl:template match="group">
  <xsl:param name="previous" select="'closing'"/>
  <xsl:if test="($previous = 'normal') or ($previous = 'closing')">
   <xsl:text>&#x20;</xsl:text>
  </xsl:if>

  <xsl:text>[</xsl:text>
  <xsl:for-each select="arg">
   <xsl:apply-templates select=".">
    <xsl:with-param name="previous" select="if (position() = 1) then 'opening' else 'normal'"/>
   </xsl:apply-templates>
   <xsl:if test="not(position() = last())">
    <xsl:text>&#x20;|</xsl:text>
   </xsl:if>
  </xsl:for-each>
  <xsl:text>]</xsl:text>
 </xsl:template>

 <xsl:template name="output-arg">
  <xsl:param name="start" select="''"/>
  <xsl:param name="end" select="''"/>
  <xsl:param name="class" select="'normal-arg'"/>
  <!-- <span class="{$class}"> -->
   <xsl:value-of select="$start"/>
   <xsl:choose>
    <xsl:when test="count(*) > 0">
     <xsl:for-each select="arg">
      <xsl:apply-templates select=".">
       <xsl:with-param name="previous" select="if (position() = 1) then 'opening' else 'normal'"/>
      </xsl:apply-templates>
     </xsl:for-each>
    </xsl:when>
    <xsl:otherwise>
     <xsl:value-of select="."/>
    </xsl:otherwise>
   </xsl:choose>
   <xsl:value-of select="$end"/>
  <!-- </span> -->
 </xsl:template>

 <xsl:template match="arg">
  <xsl:param name="previous" select="'closing'"/>
  <xsl:if test="($previous = 'normal') or ($previous = 'closing')">
   <xsl:text>&#x20;</xsl:text>
  </xsl:if>

  <xsl:choose>
   <xsl:when test="@choice = 'req'">
    <xsl:call-template name="output-arg">
     <xsl:with-param name="start" select="'{'"/>
     <xsl:with-param name="end" select="'}'"/>
     <xsl:with-param name="class" select="'required-arg'"/>
    </xsl:call-template>
   </xsl:when>

   <xsl:when test="(@choice = '') or (@choice = 'opt')">
    <xsl:call-template name="output-arg">
     <xsl:with-param name="start" select="'['"/>
     <xsl:with-param name="end" select="']'"/>
     <xsl:with-param name="class" select="'optional-arg'"/>
    </xsl:call-template>
   </xsl:when>

   <xsl:otherwise>
    <xsl:call-template name="output-arg"/>
   </xsl:otherwise>
  </xsl:choose>
 </xsl:template>
</xsl:stylesheet>
