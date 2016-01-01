# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Aravindan Mahendran
# 
# The MIT license. See LICENSE file for details

from sonar_rules_extractor.rule import Rule
from sonar_rules_extractor.extractor import RuleExtractor, ExtractorArgumentError
from HTMLParser import HTMLParser
from xml.dom.minidom import parse
import glob
import os
import sys
import re


class KlocworkExtractor(RuleExtractor):
    '''
    Extractor for Klocwork 9.2
    '''
    
    severities = {'1':'BLOCKER', '2':'BLOCKER', '3':'CRITICAL', '4':'CRITICAL', '5':'MAJOR', '6':'MAJOR', '7':'MINOR', '8':'MINOR', '9':'INFO', '10':'INFO'}
    tool = 'klocwork'

    def extract(self, *args):
        if len(args) == 0:
            raise ExtractorArgumentError('Not enough arguments. At least one folder containing Klocwork rules in HTML files has to be given')
        
        if not os.path.isdir(args[0]):
            raise ExtractorArgumentError('The first argument is not a folder.')
        
        rules = self.get_rules_from_html(args[0])
        
        if len(args)==2 and os.path.isdir(args[1]): #html folder + xml folder
            rules.extend(self.get_rules_from_xml(args[1]))
        
        elif len(args) == 2 and os.path.isfile(args[1]): #html folder + config_file
            rules = self.get_enabled_rules(rules, args[1])
        
        elif len(args) == 3:
            if not os.path.isdir(args[1]):
                raise ExtractorArgumentError('%s is not a folder' % args[1])
            elif not os.path.isfile(args[2]):
                raise ExtractorArgumentError('%s is not a file' % args[2])
            
            rules.extend(self.get_rules_from_xml(args[1]))
            rules = self.get_enabled_rules(rules, args[2])
            
        elif len(args)!=1 :
            raise ExtractorArgumentError('Bad parameters given')

        return rules
    
    def get_rules_from_html(self, html_folder_path):
        rules=[]
        for file_name in glob.glob(os.path.join(html_folder_path,"*.htm*")):
            html_parser = KlocworkHTMLParser()
            fd = open(file_name,'r')
            lines_list = []
            for line in fd.readlines():
                lines_list.append(line.replace('\n',''))
            fd.close()
            html_parser.feed(''.join(lines_list))
            rules.extend(html_parser.rules)
        return rules
    
    def get_rules_from_xml(self, xml_folder_path):
        rules=[]
        for file_name in glob.glob(os.path.join(xml_folder_path,"*.xml")):
            fd = open(file_name, 'r')
            node_list = parse(fd)
            rules.extend(handle_klocwork_xml(node_list))
            fd.close()
        return rules
    
    def get_enabled_rules(self, rules, config_file_path):
        fd = open(config_file_path)
        node_list = parse(fd)
        enabled_rules = handle_klocwork_config_file(node_list, rules)
        fd.close()
        if len(enabled_rules)==0:
            return rules
        else:
            return enabled_rules
        
class KlocworkHTMLParser(HTMLParser):
    '''
    A HTML parser which extracts klocwork rules infos (key and description)
    '''
    
    def __init__(self):
        HTMLParser.__init__(self)
        self.is_in_table = False
        self.is_in_tr = False
        self.is_in_td = False
        self.is_in_a = False
        self.current_rule = Rule(None, None, None, None, None, None)
        self.rules = []
    
    def handle_starttag(self, tag, attrs):
        if tag == 'table':
            for element in attrs:
                if element[0] == 'id' and element[1]== 'sortable_table_id_0':
                    self.is_in_table = True
        elif self.is_in_table and tag == 'tr':
            self.is_in_tr = True
        elif self.is_in_tr and tag == 'td':
            self.is_in_td = True
        elif self.is_in_td and tag == 'a':
            self.is_in_a = True
    
    def handle_data(self, data):
        if self.is_in_a:
            self.current_rule.key=data.strip()
            self.current_rule.category='Reliability'
            self.current_rule.configKey = self.current_rule.key
        elif self.is_in_td and self.current_rule.description == None and len(data.strip()) != 0:
            self.current_rule.name=data.strip()
            self.current_rule.description=data.strip()
        elif self.is_in_td and self.current_rule.priority == None and len(data.strip()) != 0:
            self.current_rule.priority = KlocworkExtractor.severities[data.strip()]
        
    def handle_endtag(self, tag):
        if tag=='table' and self.is_in_table:
            self.is_in_table=False
        elif tag=='td' and self.is_in_td:
            self.is_in_td = False
        elif tag=='tr' and self.is_in_tr:
            self.is_in_tr = False
            if self.current_rule.key!=None:
                self.rules.append(self.current_rule)
            self.current_rule = Rule(None, None, None, None, None, None)
        elif tag=='a' and self.is_in_a:
            self.is_in_a = False
 

def handle_klocwork_xml(checkers_node):
    rules = []
    checkers = checkers_node.getElementsByTagName('checker')
    for checker in checkers:
        for error in checker.getElementsByTagName('error'):
            rule = Rule(error.getAttribute('id'),error.getAttribute('title'), error.getAttribute('id'), error.getAttribute('title'), KlocworkExtractor.severities[error.getAttribute('severity')])
            rules.append(rule)
    return rules

def handle_klocwork_config_file(errors_node, rules):
    enabled_rules = []
    for error in errors_node.getElementsByTagName('error'):
        for rule in rules:
            if error.getAttribute('enabled') == 'true' and error.getAttribute('id') == rule.key:
                new_rule = Rule(rule.key,rule.name,rule.configKey,rule.description,rule.priority)
                if error.getAttribute('severity') != None and len(error.getAttribute('severity').strip()) != 0:
                    print error.getAttribute('id')
                    new_rule.priority = KlocworkExtractor.severities[error.getAttribute('severity')]
                enabled_rules.append(new_rule)
                
    return enabled_rules

    
