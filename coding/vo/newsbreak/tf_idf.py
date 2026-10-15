# The TF-IDF equation is as follows:
# TF (Term Frequency) = (Number of occurrences of a term in a document) / (Total number of terms in the document)
# IDF (Inverse Document Frequency) = log((Total number of documents) / (Number of documents containing the term))
# TF-IDF = TF * IDF

# Write a function, calculate the tf-idf of top K words in each document, print them out in following format

# 0, word1, tfidf
# 0, word2, tfidf
# 0, word3, tfidf
# 1, word1, tfidf
# 1, word2, tfidf
# 1, word3, tfidf
# 2, word1, tfidf
# 2, word2, tfidf
# 2, word3, tfidf

import math
from collections import Counter
from heapq import nlargest

articles = [
    "stretches of history terminating an unwanted pregnancy especially in the early stages was a relatively uncontroversial fact of life historians say Egyptian papyrus Greek plays Roman coins the medieval biographies of saints medical and midwifery manuals and Victorian newspaper and pamphlets reveal that abortion was more common in premodern times than people might think",
    "When asked whether any remains may be recovered Mauger noted the incredibly unforgiving environment adding I dont have an answer for prospects at this time A medical expert said a deep-sea implosion would leave behind no recoverable remains Once the search began crews had sonar buoys in the water nearly continuously and did not detect any catastrophic events Mauger said",
    "His 1989 film The Abyss was set in the ocean and Cameron has said he made Titanic in part to explore the wreckI made Titanic because I wanted to dive to the shipwreck not because I particularly wanted to make the movie he told Playboy in 2009 The Titanic was the Mount Everest of shipwrecks and as a diver I wanted to do it right When I learned some other guys had dived to the Titanic to make an IMAX movie I said Ill make a Hollywood movie to pay for an expedition and do the same thing I loved that first taste and I wanted more",
    "The small caps rally is also an auspicious sign for the broader economy says Quincy Krosby chief global strategist at LPL Financial Because small caps tend to be more volatile their rally suggests that investors risk appetites are growing and theyre looking past the banking turmoil earlier this yearIf we continue to see interest in the small caps it would reflect investor belief that it will be a more muted recession said Krosby",
    "During a signing ceremony in Paris on Thursday Airbus CEO Guillaume Faury and the NDRCs head Zheng Shanjie pledged to accelerate the construction of Airbus new assembly line in the Chinese coastal city of Tianjin the NDRC said The Chinese planner said it supports domestic airlines cooperating with Airbus according to their needs",
    "The suspended production comes after employees voted down Spirit AeroSystems best and final offer and then authorized a strike according to the union The work stoppage is set to begin on Saturday",
    "The International Association of Machinists and Aerospace Workers or IAM represents about 6000 workers at the plant The contract was rejected by 79% of members and 85% voted to strike the union said",
    "We are providing you with this update in real-time to let you know the Companys intention to move forward with this sale Dixon and Lokhandwala wrote in the memo It still hasnt been finalized by the court but once it is it will mark an important milestone on the road to long-term financial health and stability for VMG",
    "Under new ownership Dixon and Lokhandwala added we look forward to a new chapter in VMGs history with a renewed focus and commitment to creating world-class content for our audiences and partners"
]

# [{word1: tf-idf, word2: tf-idf}, {}]

def get_tf_idf(articles, k):
    N = len(articles)
    tokenized_docs = [doc.strip().lower().split() for doc in articles]
    tf_list = []
    for doc in tokenized_docs:
        counter = Counter(doc)
        total = len(doc)
        tf = {word: count / total for word, count in counter.items()}
        tf_list.append(tf)
    df = Counter()
    for tf in tf_list:
        for word in tf.keys():
            df[word] += 1
    idf = {word: math.log(N / df_val) for word, df_val in df.items()}
    
    for i, tf in enumerate(tf_list):
        tfidf = {word: tf[word] * idf[word] for word in tf}
        topk = nlargest(k, tfidf.items(), key=lambda x: x[1]) # heapsize = k, O(nlogk)
        for word, val in topk:
            print(f"{i}, {word}, {val}")
    
get_tf_idf(articles, 3)
    


