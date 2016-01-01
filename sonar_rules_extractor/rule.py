# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Robin Jarry
# 
# The MIT license. See LICENSE file for details

from xml.dom import minidom

class Rule:

    def __init__(self, key='', name='', configKey='', description='', 
                 priority='INFO', category='Reliability'):
        self.key = key
        self.priority = priority
        self.name = name
        self.configKey = configKey
        self.category = category
        self.description = description
    
    def __str__(self):
        return '[%s] %s' % (self.key, self.name)
    
    def to_xml_element(self):
        rule_elt = minidom.Element('rule')
        rule_elt.setAttribute('key', self.key)
        rule_elt.setAttribute('priority', self.priority)
        
        name_elt = minidom.Element('name')
        name_cdata = minidom.CDATASection()
        name_cdata.data = self.name
        name_elt.appendChild(name_cdata)
        
        configKey_elt = minidom.Element('configKey')
        configKey_cdata = minidom.CDATASection()
        configKey_cdata.data = self.configKey
        configKey_elt.appendChild(configKey_cdata)
        
        category_elt = minidom.Element('category')
        category_elt.setAttribute('name', self.category)
        
        description_elt = minidom.Element('description')
        description_cdata = minidom.CDATASection()
        description_cdata.data = self.description
        description_elt.appendChild(description_cdata)
        
        rule_elt.appendChild(name_elt)
        rule_elt.appendChild(configKey_elt)
        rule_elt.appendChild(category_elt)
        rule_elt.appendChild(description_elt)
        
        return rule_elt
