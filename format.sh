set -e
for f in $(find src include examples test benchmarks -path src/data -prune -o -type f -print); do
	clang-format-10 -style=file -i $f
done
cd data_processor_julia
julia  -e 'import JuliaFormatter;JuliaFormatter.format(".")'
