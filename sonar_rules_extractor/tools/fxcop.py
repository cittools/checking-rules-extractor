# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Aravindan Mahendran
# 
# The MIT license. See LICENSE file for details

from sonar_rules_extractor.rule import Rule
from sonar_rules_extractor.extractor import RuleExtractor, ExtractorArgumentError
from xml.dom.minidom import parseString
import glob
import os

            
class FxCopExtractor(RuleExtractor):
    '''
    Extractor for FxCop 10.0
    '''
    
    severities = {'CriticalError':'BLOCKER', 'Error':'CRITICAL', 'CriticalWarning':'MAJOR', 'Warning':'MINOR', 'Information':'INFO'}
    tool = 'fxcop'
    
    def extract(self, *args):
        rules = []
        if len(args) == 0:
            raise ExtractorArgumentError('Not enough arguments. A XML file containing cppcheck rules has to be given')
        
        if not os.path.isdir(args[0]):
            raise ExtractorArgumentError('The given path is not a directory.')
        
        for file_name in glob.glob(os.path.join(args[0],"*.dll")):
            xml = self.extract_xml(file_name)
            rules.extend(self.handle_xml_string(xml))
        
        return rules
    
    def handle_xml_string(self, string_to_parse):
        rules = []
        node_list = parseString(string_to_parse)
        for rule_tag in node_list.getElementsByTagName('Rule'):
            key = rule_tag.getAttribute('TypeName')
            name =  rule_tag.getElementsByTagName('Name')[0].firstChild.data
            name = name.encode('ascii', 'ignore')
            description = rule_tag.getElementsByTagName('Description')[0].firstChild
            if description == None:
                description = name
            else:
                description = rule_tag.getElementsByTagName('Description')[0].firstChild.data
                description = description.encode('ascii', 'ignore')
            fxcop_severity = rule_tag.getElementsByTagName('MessageLevel')[0].firstChild.data
            if fxcop_severity in self.severities:
                severity = self.severities[fxcop_severity]
            else :
                severity = self.severities['Warning']
            rule = Rule(key,name,key, description, severity)
            rules.append(rule)
        
        return rules
    
    def extract_xml(self, file_name):
        begin_tag = '<Rules'
        end_tag = '</Rules>'
        xml = ''
        is_in_xml = False
        fd = open(file_name, 'rb')
        for line in fd.readlines():
            if line.find(begin_tag)!=-1 and not is_in_xml:
                is_in_xml=True
                xml += line[line.find(begin_tag):]
            elif line.find(end_tag)!=-1 and is_in_xml:
                xml += line[:line.find(end_tag) + len(end_tag)]
                break
            elif is_in_xml :
                xml+= line

        fd.close()
        return xml
