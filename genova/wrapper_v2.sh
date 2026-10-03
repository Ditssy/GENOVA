echo "=== GENOVA V2 EXPERIMENT ===" > media/log.txt
for i in $(seq 1 10); do
  echo "--- run $i ---" >> media/log.txt
  taskset -c 2 ./genova_pi_v2 >> media/log.txt 2>&1
done
