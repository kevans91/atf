<?xml version="1.0" encoding="ISO-8859-1"?>
<!DOCTYPE xsl:stylesheet [<!ENTITY nbsp "&#160;">]>

<xsl:stylesheet version="1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <!-- ****************************************************************
       Root templates.
       **************************************************************** -->

  <xsl:template match="/">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()" />
    </xsl:copy>
  </xsl:template>

  <xsl:template match="article">
    <html xmlns="http://www.w3.org/1999/xhtml">
      <head>
        <xsl:call-template mode="top" name="generate-html-head" />
      </head>

      <body>
        <xsl:call-template mode="top" name="generate-html-body" />
      </body>
    </html>
  </xsl:template>

  <!-- ****************************************************************
       Templates for mode="top".
       **************************************************************** -->

  <xsl:template mode="top" match="*" priority="-1">
    <xsl:text>UNMANAGED ELEMENT</xsl:text>
  </xsl:template>

  <xsl:template mode="top" name="generate-html-head">
    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1" />

    <link rel="made" href="mailto:atf-devel AT NetBSD DOT org" />
    <link rel="stylesheet" type="text/css" href="standalone.css" />

    <xsl:apply-templates mode="html-head" />
  </xsl:template>

  <xsl:template mode="top" name="generate-html-body">
    <div class="header">
      <xsl:apply-templates mode="header" />
    </div>

    <div class="contents">
      <xsl:apply-templates mode="contents">
        <xsl:with-param name="depth" select="0" />
      </xsl:apply-templates>
    </div>
  </xsl:template>

  <!-- ****************************************************************
       Templates for mode="html-head".
       **************************************************************** -->

  <xsl:template mode="html-head" match="*" priority="-1">
    <xsl:text>UNMANAGED ELEMENT</xsl:text>
  </xsl:template>

  <xsl:template mode="html-head" match="articleinfo">
    <title><xsl:value-of select="title" /></title>
  </xsl:template>

  <xsl:template mode="html-head" match="note|section|para" />

  <!-- ****************************************************************
       Templates for mode="header".
       **************************************************************** -->

  <xsl:template mode="header" match="*" priority="-1">
    <xsl:text>UNMANAGED ELEMENT</xsl:text>
  </xsl:template>

  <xsl:template mode="header" match="articleinfo">
    <p class="title">
      <xsl:apply-templates mode="common" select="title" />
    </p>
    <p class="author">
      <xsl:apply-templates mode="common" select="author" />
    </p>
  </xsl:template>

  <xsl:template mode="header" match="note|section|para" />

  <!-- ****************************************************************
       Templates for mode="contents".
       **************************************************************** -->

  <xsl:template mode="contents" match="*" priority="-1">
    <xsl:text>UNMANAGED ELEMENT</xsl:text>
  </xsl:template>

  <xsl:template mode="contents" match="articleinfo" />

  <xsl:template mode="contents" match="emphasis">
    <xsl:choose>
      <xsl:when test="@role = 'strong'">
        <b><xsl:apply-templates mode="contents" /></b>
      </xsl:when>
      <xsl:otherwise>
        <i><xsl:apply-templates mode="contents" /></i>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template mode="contents" match="filename">
    <tt class="filename"><xsl:apply-templates mode="contents" /></tt>
  </xsl:template>

  <xsl:template mode="contents" match="itemizedlist">
    <ul>
      <xsl:apply-templates mode="contents" />
    </ul>
  </xsl:template>

  <xsl:template mode="contents" match="listitem">
    <li>
      <xsl:apply-templates mode="contents" />
    </li>
  </xsl:template>

  <xsl:template mode="contents" match="note">
    <div class="note">
      <xsl:apply-templates />
    </div>
  </xsl:template>

  <xsl:template mode="contents" match="orderedlist">
    <ol>
      <xsl:apply-templates mode="contents" />
    </ol>
  </xsl:template>

  <xsl:template mode="contents" match="para">
    <p xml:space="preserve"><xsl:apply-templates mode="contents" /></p>
  </xsl:template>

  <xsl:template mode="contents" match="section">
    <xsl:param name="depth" />

    <xsl:choose>
      <xsl:when test="$depth = 0">
        <h1><xsl:apply-templates mode="common" select="title" /></h1>
      </xsl:when>
      <xsl:when test="$depth = 1">
        <h2><xsl:apply-templates mode="common" select="title" /></h2>
      </xsl:when>
      <xsl:when test="$depth = 2">
        <h3><xsl:apply-templates mode="common" select="title" /></h3>
      </xsl:when>
      <xsl:when test="$depth = 3">
        <h4><xsl:apply-templates mode="common" select="title" /></h4>
      </xsl:when>
      <xsl:when test="$depth = 4">
        <h5><xsl:apply-templates mode="common" select="title" /></h5>
      </xsl:when>
      <xsl:when test="$depth = 5">
        <h6><xsl:apply-templates mode="common" select="title" /></h6>
      </xsl:when>
      <xsl:otherwise>
        <!-- XXX Can we raise an error from XSLT? -->
        <h6>ERROR; SECTION NESTING TOO DEEP</h6>
      </xsl:otherwise>
    </xsl:choose>

    <xsl:apply-templates mode="contents" select="*[position() &gt; 1]">
      <xsl:with-param name="depth" select="$depth + 1" />
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template mode="contents" match="literal">
    <tt><xsl:apply-templates mode="contents" /></tt>
  </xsl:template>

  <!-- ****************************************************************
       Common templates.
       **************************************************************** -->

  <xsl:template mode="common" match="*" priority="-1">
    <xsl:text>UNMANAGED ELEMENT</xsl:text>
  </xsl:template>

  <xsl:template mode="common" match="author">
    <xsl:apply-templates select="firstname" />
    <xsl:text> </xsl:text>
    <xsl:apply-templates select="surname" />
  </xsl:template>

  <xsl:template mode="common" match="title">
    <xsl:apply-templates />
  </xsl:template>

</xsl:stylesheet>

<!--
  vim: syntax=xslt:expandtab:shiftwidth=2:softtabstop=2
  -->
