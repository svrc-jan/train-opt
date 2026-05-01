app="build/conflict_solve"

for file in data/*/*.json
do
    #echo $file
    $app "$file"
done


