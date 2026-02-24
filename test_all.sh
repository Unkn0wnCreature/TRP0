SERVER_PORT=8080
SERVER_PID=""

check_files() {
	if [[ ! -f "test.txt" ]]; then
		echo -e "\n"
		exit 1
	fi
}

compile() {
	echo -e "\n"
	g++ server.cpp matrix.h graph.h -o server
	g++ client.cpp matrix.h file_manager.h -o client
	if [[ $? -ne 0 ]]; then
		echo -e ""
		exit 1
	fi
}

start_server() {
	local proto=$1
	echo -e ""
	./server $SERVER_PORT $protocol &
      	SERVER_PID=$!
      	sleep 1
}

stop_server() {
	if [[ -n "$SERVER_PID" ]]; then
		echo -e ""
		kill $SERVER_PID 2>/dev/null
		wait $SERVER_PID 2>/dev/null
		SERVER_PID=""
	fi
}

run_test() {
	local desc=$1
	local protocol=$2
	local source=$3
	local param=$4
	local graph_data=$5
	local start_end=$6
	local expected_msg=$7
	local expected_path=$8

	echo -n ""
	./test_client.exp "127.0.0.1" "$SERVER_PORT" "$protocol" "$source" "$param" "$graph_data" "$start_end" "$expected_msg" "$expected_path" > /dev/null

	if [[ $? -eq 0 ]]; then
		echo -e "";
	else
		echo -e "";
	fi
}

DATA_6="0 1 0 0 0 0|1 0 1 0 0 0|0 1 0 1 0 0|0 0 1 0 1 0|0 0 0 1 0 1|0 0 0 0 1 0"
S_E_6="1 6"
PATH_6="1 2 3 4 5 6"

compile
chmod +x test_client.exp

start_server "tcp"

run_test "N = 6" "tcp" "2" "6" "$DATA_6" "$S_E_6" "5" "$PATH_6"
stop_server
