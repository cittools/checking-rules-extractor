# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Aravindan Mahendran
# 
# The MIT license. See LICENSE file for details

from sonar_rules_extractor.rule import Rule
from sonar_rules_extractor.extractor import RuleExtractor, ExtractorArgumentError
import os
import glob
import re
import sys
import __main__


class Gnatcheck(RuleExtractor):
    
    tool = 'gnatcheck'
    severities = {'Required':'BLOCKER', 'Advisory':'CRITICAL'}
    markups = {'Applied rules:':False,'Disabled rules:':False,'Checked argument sources:':False,'-------- Start Section 1 ------------':False,'-------- End Section 1 ------------':False,'-------- Start Section 2 ------------':False,'-------- End Section 2 ------------':False,'-------- Start Section 3 ------------':False,'-------- End Section 3 ------------':False}
    
    def extract(self, *args):
        if len(args) == 0:
            raise ExtractorArgumentError('Not enough arguments. A Gnatcheck output containing your rules has to be given.')
        
        if not os.path.isfile(args[0]):
            raise ExtractorArgumentError('The given argument is not a file.')
        
        if not self.validate_input_file(args[0]):
            raise ExtractorArgumentError('The given argument is not a valid Gnatcheck output.')
        
        return self.get_rules(args[0])
    
    def validate_input_file(self, file_name):
        fd = open(file_name,'r')
        for line in fd.readlines():
            if line.strip() in self.markups:
                self.markups[line.strip()] = True
        fd.close()
        
        if False in self.markups.itervalues():
            return False
        
        return True
    
    def get_rules(self, file_name):
        fd = open(file_name,'r')
        in_rules = False
        rules = []
        for line in fd.readlines():
            line = line.strip()
            if line == 'Applied rules:' or line == 'Disabled rules:':
                in_rules = True
            elif line in self.markups:
                break
            elif in_rules:
                pattern = re.compile('(\\([^\\)]*\\))\\s(.*)')
                search = pattern.search(line)
                if search != None:
                    values = search.groups()
                    rules.append(Rule(key=values[0], name=values[1], description=values[1], configKey=values[1]))
        fd.close()
        
        return rules
    
class Gnatcheck632(RuleExtractor):
    tool = 'gnatcheck632'
    severities = {'Required':'BLOCKER', 'Advisory':'CRITICAL'}
    
    def __init__(self):
        self.rules =[]
        self.finished = False
        self.first_line = True
    
    def extract(self, *args):
        if len(args) == 0:
            input_stream=sys.stdin
        
        elif not os.path.isfile(args[0]):
            raise ExtractorArgumentError('The first argument is not a file.')
        
        else:
            input_stream=open(args[0],'r')
            
        for line in input_stream.readlines():
            self.fill_rules(line)
        
        return self.rules
        
    def fill_rules(self, line):
        if self.first_line :
            self.first_line = False
            
        elif self.finished:
            pass
        
        elif line.startswith('  '):
            rule = self.rules.pop()
            rule.description += ' ' + line.strip()
            rule.name = rule.description
            rule.configKey = rule.description
            self.rules.append(rule)
            
                
        elif line.startswith(' '):
            tokens = line.strip().split(' - ')
            self.rules.append(Rule(key=tokens[0], name=tokens[1], configKey=tokens[1], description=tokens[1], 
                 priority='INFO', category='Reliability'))
        else:
            self.finished=True
        