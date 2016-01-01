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

name_length = 192

class Logiscope(RuleExtractor):
    
    tool = 'logiscope'
    severities = {'Required':'BLOCKER', 'Advisory':'CRITICAL'}
    user_severities = {}
    max_int_severity = 100
    
    def extract(self, *args):
        if len(args) == 0:
            raise ExtractorArgumentError('Not enough arguments. At least one folder containing .rl or .std files has to be given')
        
        if not os.path.isdir(args[0]):
            raise ExtractorArgumentError('%s is not a folder.' % args[0])
        
        rules = []
        
        for file_name in glob.glob(os.path.join(args[0],"*.rst")):
            self.read_rst_files(file_name)
        
        for file_name in glob.glob(os.path.join(args[0],"*.rl")):
            if self.validate_input_file(file_name):
                rules.append(self.extract_rules(file_name))
            else:
                print >> sys.stderr, '%s is not a valid file' % file_name
                
        for file_name in glob.glob(os.path.join(args[0],"*.std")):
            if self.validate_input_file(file_name):
                rules.append(self.extract_rules(file_name))
            else:
                print >> sys.stderr, '%s is not a valid file' % file_name
            
        return rules
    
    
    
    def validate_input_file(self, file_path):
        fd = open(file_path, 'r')
        name_found = False
        severity_found = False
        for line in fd.readlines():
            if line.startswith('.NAME'):
                name_found = True
            elif line.startswith('.SEVERITY'):
                severity_found = True
            
            if name_found and severity_found:
                break
        
        fd.close()
        
        return name_found and severity_found
    
    def read_rst_files(self, file_path):
        fd = open(file_path, 'r')
        for line in fd.readlines():
            matcher = re.match('SEVERITY_VALUE \"?([\w ]+?)\"? (\d+?) END SEVERITY_VALUE', line)
            if matcher != None:
                severity_level = matcher.group(2)
                severity_name = matcher.group(1)
                if severity_level > self.max_int_severity:
                    self.max_int_severity = severity_level
                
                self.user_severities[severity_name] = severity_level
            
        fd.close()
        
        #Parcours de user_severities et calcul
        for severity in self.user_severities.keys():
            self.user_severities[severity] = self.calculate_sonar_severity(self.user_severities[severity])
            print severity, self.user_severities[severity]
    
    def extract_rules(self, file_path):
        fd = open(file_path, 'r')
        std_description = False
        rl_description = False
        rule = Rule(None, None, None, None, None, None)
        splittedPath = file_path.split(os.sep)
        rule.key = splittedPath[-1].split('.')[0]
        rule.configKey = rule.key
        rule.category = "Reliability"
        rule.name =''
        for line in fd.readlines():
            if std_description:
                if not len(line)==0 and not line.startswith('---') and not line.startswith('Definition:'):
                    rule.name += line
            
            #In .rl files, most of the time, the first line after the ".TITLE Description" Markup is enough to understand the violation...
            elif rl_description:
                rule.name += line
                #...But in some .rl files, we have to copy more lines
                if not line.endswith(':') and not line.startswith('-'):
                    rl_description=False
                    
            if line.startswith('.NAME '):
                rule.description = line[len('.NAME ')-1:]
            
            elif line.startswith('.SEVERITY '):
                severity = line[len('.SEVERITY ')-1:].strip()
                if severity in self.user_severities.keys():
                    rule.priority = self.user_severities[severity]
                elif severity in self.severities.keys():
                    rule.priority = self.severities[severity]
                elif re.match("\d+", severity):
                    rule.priority = self.calculate_sonar_severity(severity)
                else:
                    rule.priority = 'INFO'
            
            elif line.startswith('.DESCRIPTION'):
                std_description = True
                
            elif line.startswith('.TITLE Description'):
                rl_description = True
                
        if std_description:
            #Trying to get characters until ". " is met
            if rule.name.find('. ') != -1:
                rule.name = rule.name[:rule.name.find('. ')]
            
            #Trying to get characters until "." is met
            else:
                rule.name = rule.name[:rule.name.find('.')]
            
            #Last problem : sometimes, the above verification is not enough strengthful. So we have to check that there isn't more than the description in the string
            if rule.name.find(".SEVERITY") != -1:
                rule.name = rule.name[:rule.name.find(".SEVERITY")]
        
        rule.name = rule.name.strip()
        
        if (len(rule.name)>= name_length):
            rule.name = rule.name[:name_length-1]
        
        fd.close()
        
        return rule
    
    def calculate_sonar_severity(self, int_severity):
        level = int(self.max_int_severity) / 5
        severity = "INFO"
        int_severity = int(int_severity)
        if int_severity <= level:
            severity = "BLOCKER"
        elif int_severity <= 2*level:
            severity = "CRITICAL"
        elif int_severity <= 3*level:
            severity = "MAJOR"
        elif int_severity <= 4*level:
            severity = "MINOR"
        elif int_severity <= 5*level:
            severity = "INFO"
        
        return severity

class Logiscope66(Logiscope):
    
    tool = 'logiscope66'
    severities = {'Required':'BLOCKER', 'Advisory':'CRITICAL'}
    user_severities = {}
    max_int_severity = 100
    
    def extract(self, *args):
        if len(args) == 0:
            raise ExtractorArgumentError('Not enough arguments. At least one folder containing .rl or .std files has to be given')
        
        if not os.path.isdir(args[0]):
            raise ExtractorArgumentError('%s is not a folder.' % args[0])
        
        rules = []
        
        for file_name in glob.glob(os.path.join(args[0],"*.rst")):
            self.read_rst_files(file_name)
        
        for file_name in glob.glob(os.path.join(args[0],"*.rl")):
            if self.validate_input_file(file_name):
                rules.append(self.extract_rules(file_name))
            else:
                print >> sys.stderr, '%s is not a valid file' % file_name
                
        for file_name in glob.glob(os.path.join(args[0],"*.std")):
            if self.validate_input_file(file_name):
                rules.append(self.extract_rules(file_name))
            else:
                print >> sys.stderr, '%s is not a valid file' % file_name
            
        return rules
    
    def extract_rules(self, file_path):
        fd = open(file_path, 'r')
        std_description = False
        rl_description = False
        rule = Rule(None, None, None, None, None, None)
        splittedPath = file_path.split(os.sep)
        rule.key = splittedPath[-1].split('.')[0]
        rule.configKey = rule.key
        rule.category = "Reliability"
        rule.name =''
        for line in fd.readlines():
            if std_description:

                if not len(line)==0 and not line.startswith('---') and not line.startswith('Definition:'):
                    rule.name += line
            
            elif rl_description:
                
                if line.startswith('Example:') or line.startswith('.'):
                    rl_description=False
                else :
                    rule.name = rule.name.strip() + " "
                    rule.name += line
                    
            if line.startswith('.SEVERITY '):
                severity = line[len('.SEVERITY ')-1:].strip()
                if severity in self.user_severities.keys():
                    rule.priority = self.user_severities[severity]
                elif severity in self.severities.keys():
                    rule.priority = self.severities[severity]
                elif re.match("\d+", severity):
                    rule.priority = self.calculate_sonar_severity(severity)
                else:
                    rule.priority = 'INFO'
            
            elif line.startswith('.DESCRIPTION'):
                std_description = True
                
            elif line.startswith('.TITLE Description'):
                rl_description = True
                
        if std_description:
            #Trying to get characters until "." is met
            if rule.name.find('.') != -1:
                rule.name = rule.name[:rule.name.find('.')]
            
            #Last problem : sometimes, the above verification is not enough strengthful. So we have to check that there isn't more than the description in the string
            if rule.name.find(".SEVERITY") != -1:
                rule.name = rule.name[:rule.name.find(".SEVERITY")]
        
        rule.name = rule.name.strip()
        rule.description = rule.name
        
        if (len(rule.name)>= name_length):
            rule.name = rule.name[:name_length-1]
        
        fd.close()
        
        return rule
    
if __name__ == "__main__":
    rules = Logiscope66().extract_rules('C:/Program Files/IBM/Rational/Logiscope/6.6/Ref/Rules/C/MISRA_Rule96ii.rl')