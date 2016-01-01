<?xml version="1.0" encoding="UTF-8"?>

<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:xs="http://www.w3.org/2001/XMLSchema" xmlns:fn="http://www.w3.org/2005/xpath-functions">
    <xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>

    <xsl:template match="results">

        <xsl:element name="rules">
        
        		
                        
                <xsl:for-each-group select="error" group-by="@id">
                
                <xsl:for-each select="distinct-values(./@id)">
                                	
					<xsl:for-each select="current-group()">
																												
	                    <xsl:element name="rule">
	                    
	                        <xsl:attribute name="key">
	                            <xsl:value-of select="@id"/>
	                        </xsl:attribute>
	                        
	                        <xsl:attribute name="priority">
	                            <xsl:value-of select="@severity"/>
	                        </xsl:attribute>
	                                                                                               
	                        <xsl:element name="name">
	                        	<xsl:text disable-output-escaping="yes">&lt;![CDATA[</xsl:text>
	                        	<xsl:value-of select="@id"/>
	                        	<xsl:text disable-output-escaping="yes">]]&gt;</xsl:text>
	                        </xsl:element>
	                        
	                        <xsl:element name="configKey">
	                        	<xsl:text disable-output-escaping="yes">&lt;![CDATA[</xsl:text>
	                        	<xsl:value-of select="@id"/>
	                        	<xsl:text disable-output-escaping="yes">]]&gt;</xsl:text>
	                        </xsl:element>
	
	                        <xsl:element name="category">
	                            <xsl:attribute name="name">
	                                <xsl:text>Reliability</xsl:text>
	                            </xsl:attribute>
	                        </xsl:element>
	                        
	                        <xsl:element name="description">
	                        	<xsl:text disable-output-escaping="yes">&lt;![CDATA[</xsl:text>
	                        	<xsl:value-of select="@msg"/>
	                        	<xsl:text disable-output-escaping="yes">]]&gt;</xsl:text>
	                        </xsl:element>	                                                                   
	                        
	                    </xsl:element>
	                                       	                                       	                 
                    </xsl:for-each>  
                    
                    </xsl:for-each>                                        

              </xsl:for-each-group>
            
        </xsl:element>
    </xsl:template>
</xsl:stylesheet>