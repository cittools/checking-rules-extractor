# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Aravindan Mahendran
# 
# The MIT license. See LICENSE file for details

from sonar_rules_extractor.rule import Rule
from sonar_rules_extractor.extractor import RuleExtractor, ExtractorArgumentError
import os
import re


class PRQA(object):
    '''
    This class contains the common methods to parse a PRQA message file
    '''
    
    #Severities in QAC/QACPP
    severities = {'0':'INFO','1':'INFO','2':'MINOR','3':'MINOR','4':'MINOR','5':'MAJOR','6':'MAJOR','7':'MAJOR','8':'CRITICAL','9':'BLOCKER'}
    
    def __init__(self):
        self.sub_severities = {}
        
    def extract(self, tool_markup, *args):
        if len(args) == 0:
            raise ExtractorArgumentError('Not enough arguments. At least one '+tool_markup+' message file has to be given.')
        
        input_file_name = args[0]
        
        user_file_name = None
        if len(args)>1:
            user_file_name = args[1]
        
        if not os.path.isfile(input_file_name) or not input_file_name.endswith(('.msg', '.usr.ex')):
            raise ExtractorArgumentError('The first path does not correspond to a '+tool_markup+' message file.')
        
        if user_file_name and (not os.path.isfile(user_file_name) or not user_file_name.endswith(('.msg', '.usr.ex'))):
            raise ExtractorArgumentError('The second path does not correspond to a '+tool_markup+' message file.')
        
        rules = self.extract_rules(tool_markup, input_file_name)
        
        rules_user = None
        if user_file_name:
            rules_user = self.extract_rules(tool_markup, user_file_name)
            
        if rules_user:
            rules.extend(rules_user)
        return rules
        
    
    def extract_rules(self, tool_markup, input_file_name):
        same_rule = False
        previous_rule = None
        rules = []
        fd = open(input_file_name, 'r')
        for line in fd.readlines():
            line = line.strip()
            if same_rule == True:
                previous_rule.description += " " + line
                if previous_rule.description[-1] != '\\':
                    same_rule = False
                else:
                    previous_rule.description = previous_rule.description[0:-2]
                    
                continue
            if line.startswith('*') or line.startswith('#levelname'):
                continue
            if line.startswith('#'):
                self.treat_sub_severities(line)
            else:
                line = re.sub('\\s+', ' ', line).strip()
                if line:
                    key = tool_markup+'_'+line[0:line.index(' ')]
                    line = line[line.index(' '):len(line)].strip()
                    priority = self.sub_severities[line[0:line.index(' ')]]
                    line = line[line.index(' '):len(line)].strip()
                    name = line.strip()
                    if name[-1]=='\\':
                        name = name[0:-2]
                        same_rule=True
                    
                    rule = Rule(key, name, key, name, priority)
                    rules.append(rule)
                    if same_rule==True:
                        previous_rule = rule
                    
                    
                
        fd.close()
        return rules
        
    def treat_sub_severities(self, line):
        new_line = re.sub('\\s+', ' ', line)
        components = new_line.split();
        if not components[2] in self.severities:
            severity = 'BLOCKER'
        else :
            severity = self.severities[components[2]]
        self.sub_severities[components[1]] = severity

class QAC(RuleExtractor):
    
    tool = 'qac'
    
    def extract(self, *args):
        return PRQA().extract(self.tool, *args)
    
class QACPP (RuleExtractor):
    
    tool = 'qacpp'
    
    def extract(self, *args):
        return PRQA().extract(self.tool, *args)
    
