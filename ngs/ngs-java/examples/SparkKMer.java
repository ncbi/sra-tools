/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//package examples;
//
import scala.Tuple2;
import org.apache.commons.lang.math.LongRange;

import org.apache.spark.SparkConf;
import org.apache.spark.api.java.JavaRDD;
import org.apache.spark.api.java.JavaPairRDD;
import org.apache.spark.api.java.JavaSparkContext;
import org.apache.spark.api.java.function.FlatMapFunction;
import org.apache.spark.api.java.function.Function2;
import org.apache.spark.api.java.function.Function;
import org.apache.spark.api.java.function.PairFunction;



import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.List;

import ngs.ErrorMsg;
import ngs.ReadCollection;
import ngs.ReadIterator;
import ngs.Read;


/** 
 * Computes an approximation to pi
 * Usage: SparkKMer [slices] [kmer-size]
 */



public final class SparkKMer {

  public static void main(String[] args) throws Exception {
    //Setup
    SparkConf sparkConf = new SparkConf().setAppName("SparkKMer");
    JavaSparkContext jsc = new JavaSparkContext(sparkConf);
    //Agrument parsing
	if(args.length < 2) {
	  System.err.println("Usage: SparkKMer <accession> <kmer-length>");
      System.exit(1);
    }
	final String acc =  args[0];
	final int KMER_LENGTH = Integer.parseInt(args[1]);
	
    //Check accession and split
	ReadCollection run = gov.nih.nlm.ncbi.ngs.NGS.openReadCollection ( acc );
	long numreads = run.getReadCount ();

	//Slice the job
	int chunk = 20000; /** amount of reads per 1 map operation **/
    int slices = (int)(numreads / chunk / 1);
	if(slices==0) slices=1;
	List<LongRange> sub = new ArrayList<LongRange>();
	for(long first=1;first <= numreads;){
		long last= first+chunk-1;
		if(last > numreads) last=numreads;
		sub.add(new LongRange(first,last));
		first=last+1;
	}
	System.err.println("Prepared ranges: \n"+sub);

	JavaRDD<LongRange> jobs= jsc.parallelize(sub, slices);
	//Map
	//
	JavaRDD<String> kmers = jobs.flatMap(new FlatMapFunction<LongRange, String>() {
	  ReadCollection  run=null;
	  @Override
      public Iterable<String> call(LongRange s) {
		//Executes on task nodes
		List<String>	ret = new ArrayList<String>();
		try {
			long	first = s.getMinimumLong();
			long	last  = s.getMaximumLong();
			if(run==null) {
				run = gov.nih.nlm.ncbi.ngs.NGS.openReadCollection ( acc ); 
			}
			ReadIterator it = run.getReadRange ( first, last-first+1, Read.all );
			while(it.nextRead ()){
				//iterate through fragments
				while ( it.nextFragment () ){
					String bases=it.getFragmentBases ();
					//iterate through kmers
					for(int i=0;i<bases.length()-KMER_LENGTH;i++){
						ret.add(bases.substring(i,i+KMER_LENGTH));
					}
				}
			}
		} catch ( ErrorMsg x ) {
            System.err.println ( x.toString () );
            x.printStackTrace ();
        }
		return ret;
	  }
    });
    //Initiate kmer counting;
    JavaPairRDD<String, Integer> kmer_ones = kmers.mapToPair(new PairFunction<String, String, Integer>() {
      @Override
      public Tuple2<String, Integer> call(String s) {
        return new Tuple2<String, Integer>(s, 1);
      }
    });
	//Reduce counts
	JavaPairRDD<String, Integer> counts = kmer_ones.reduceByKey(new Function2<Integer, Integer, Integer>() {
      @Override
      public Integer call(Integer i1, Integer i2) {
        return i1 + i2;
      }
    });
	//Collect the output
    List<Tuple2<String, Integer>> output = counts.collect();
    for (Tuple2<String,Integer> tuple : output) {
      System.out.println(tuple._1() + ": " + tuple._2());
    }
    jsc.stop();
  }
}
