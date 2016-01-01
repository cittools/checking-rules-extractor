# Copyright (c) 2012 Thales Global Services SAS
# 
# Author : Robin Jarry
# 
# The MIT license. See LICENSE file for details


#------------------------------------------------------------------------------
class ExtractorArgumentError(Exception):
    pass

#------------------------------------------------------------------------------
class RuleExtractor:
    """
    Abstract rule extractor. Subclasses should implement extract().
    """
    
    # This attribute should be overriden by subclasses.
    # It will be used by the command line parser to identify extractor classes.
    tool = None
    
    def extract(self, *args):
        """
        Extract the coding rules from the input arguments (specific to each 
        extractor). 
        
        Implementations of this method should return a list or Rule objects.
        
        If there is a problem with the arguments (i.e. file not found),
        ExtractorArgumentError can be raised and will be catched by the command 
        line wrapper.
        """
        raise NotImplementedError()
