#/bin/sh!

if [[ (-z "$1") || (-z "$2")]]
then
      echo "Script usage : $0 <tsp file> <folder name in data>"
else

    cd tspcc

    make tspcc

    ./tspcc -f $1 -v 0 -s 10 -t 1 -T 100 -i 5 -q 1 -Q 100 -j 25 -o "../data/$2/"

    cd ../plots

    gnuplot -c "plot.plot" "$2"
    
fi