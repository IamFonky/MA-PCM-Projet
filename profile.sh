#/bin/bash!

if $1 && $2
then

    cd tspcc

    make tspcc

    ./tspcc -f $1 -v 0 -s 5 -t 8 -T 256 -i 16 -q 64 -Q 1024 -j 128 -c 12 -o "../data/$2/"

    cd ../plots

    #gnuplot -c "plot.plot" "$2"
    
else
      echo "Script usage : $0 <tsp file> <folder name in data>"


fi