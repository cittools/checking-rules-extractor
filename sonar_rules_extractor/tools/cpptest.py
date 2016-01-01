# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Aravindan Mahendran
# 
# The MIT license. See LICENSE file for details

from sonar_rules_extractor.rule import Rule
from sonar_rules_extractor.extractor import RuleExtractor, ExtractorArgumentError
from HTMLParser import HTMLParser
import glob
import os
import sys
import re


class CpptestExtractor(RuleExtractor):
    '''
    Extractor for the cpptest 7.3 to 9.0
    '''
    
    markups = ('html', 'head', 'title', 'strong', 'pre')
    tool = 'cpptest'

    def extract(self, *args):
        '''
        *args = a folder containing cpptest rules in HTML format
        '''
        if len(args) == 0:
            raise ExtractorArgumentError('Not enough arguments. A folder\'s name containing C++Test Rules in HTML format must be given as argument')
        
        if not os.path.isdir(args[0]):
            raise ExtractorArgumentError('The given path is not a folder.')
        
        rules = []
        for file_name in glob.glob(os.path.join(args[0],"*.html")):
            if self.validate_input_file(file_name) == True:
                rule_infos = self.extract_rule_info(file_name)
                if rule_infos != None:
                    name = rule_infos[0].strip()
                    full_key = rule_infos[1]
                    key = full_key[0:-2]
                    key_priority = full_key[-1]
                    if key_priority == '1':
                        priority = 'BLOCKER'
                    elif key_priority == '2':
                        priority = 'CRITICAL'
                    elif key_priority == '3':
                        priority = 'MAJOR'
                    elif key_priority == '4':
                        priority = 'MINOR'
                    else:
                        priority = 'INFO'
                    
                    rules.append(Rule(key, name, name, name, priority))
        
        return rules
                    

    
    def validate_input_file(self, file_name):
        '''
        This function checks that the given HTML file contains the tags defined in self.markups
        '''
        html_parser = CpptestValidationHTMLParser()
        fd = open(file_name,'r')
        for line in fd.readlines():
            html_parser.feed(line)
        fd.close()
        
        for markup in self.markups:
            if not markup in html_parser.found_markups:
                return False
        
        return True
    
    def extract_rule_info(self, file_name):
        '''
        This function extracts informations concerning the rule defined in the given HTML file
        '''
        html_parser = CpptestHTMLParser()
        fd = open(file_name,'r')
        for line in fd.readlines():
            html_parser.feed(line)
        fd.close()
        return html_parser.values
        
        
    
class CpptestValidationHTMLParser(HTMLParser):
    '''
    A HTML parser which add in a list the markups present in the file
    '''
    
    def __init__(self):
        HTMLParser.__init__(self)
        self.found_markups = []
    
    def handle_starttag(self, tag, attrs):
        if not tag in self.found_markups:
            self.found_markups.append(tag)
        
    
class CpptestHTMLParser(HTMLParser):
    '''
    A HTML parser which extracts cpptest rules infos (key and description)
    '''
    
    def __init__(self):
        HTMLParser.__init__(self)
        self.is_in_strong = False
        self.values = None
        self.first_strong_passed = False
    
    def handle_starttag(self, tag, attrs):
        if tag == 'strong':
            self.is_in_strong = True
    
    def handle_data(self, data):
        if self.is_in_strong and self.values == None and not self.first_strong_passed:
            pattern = re.compile('(.*)\\[([^\\s]{3,})\\]')
            search = pattern.search(data)
            if search != None:
                self.values = search.groups()
            
    def handle_endtag(self, tag):
        if self.is_in_strong:
            self.first_strong_passed=True
        
        
    
