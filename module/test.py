import subprocess
from concurrent.futures import ThreadPoolExecutor
import sys
import random

def execute_commands(commands, t):
    for command in commands:
        print('terminal ' + str(t) + ': ' + command)
        subprocess.call(command, shell=True)    

initial_commands = [
    'make',
    'sudo insmod cuckoo_hash_kernel.ko'
]

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Insert number of commands, terminals and words")
        sys.exit(1)
    try:
        NUM_COMMANDS = int(sys.argv[1])
        NUM_TERMINALS = int(sys.argv[2])
        NUM_WORDS = int(sys.argv[3])
        if NUM_COMMANDS <= 0 or NUM_TERMINALS <= 0 or NUM_WORDS <= 0:
            print('Numbers Must be positive')
            sys.exit(1)
    except ValueError:
        print("Wrong input format: Insert three integers")
        sys.exit(1)

words = []
for i in range(NUM_WORDS):
    words.append('word_' + str(i + 1))

executing_commands = []
operations = []

for t in range(NUM_TERMINALS):
    i = 0
    executing_commands_t = []
    operations_t = []
    while(i < NUM_COMMANDS):
        a = 2
        if i == 18:
            a = 1
        rand = random.randint(0, a)
        command = ''
        operations_t_c = []
        if rand == 0:
            rand_word = random.randint(0, len(words) - 1)
            command = 'echo "' + words[rand_word] + '" | sudo tee /sys/kernel/cuckoo_hash/get>/dev/null'
        if rand == 1:
            rand_word = random.randint(0, len(words) - 1)
            command = 'echo "' + words[rand_word] + '" | sudo tee /sys/kernel/cuckoo_hash/delete>/dev/null'
            operations_t_c.append('delete')
            operations_t_c.append(words[rand_word])
            operations_t.append(operations_t_c)
        if rand == 2:
            rand_word = random.randint(0, len(words) - 1)
            command = 'echo "' + words[rand_word] + '" | sudo tee /sys/kernel/cuckoo_hash/insert>/dev/null'
            executing_commands_t.append(command)
            rand_value = random.randint(0, 100)
            command = 'echo ' + str(rand_value) + ' | sudo tee /sys/kernel/cuckoo_hash/value>/dev/null'
            operations_t_c.append('insert')
            operations_t_c.append(words[rand_word])
            operations_t_c.append(rand_value)
            operations_t.append(operations_t_c)
            i += 1
        i += 1
        executing_commands_t.append(command)
    executing_commands.append(executing_commands_t)
    operations.append(operations_t)

print_command = 'sudo cat /sys/kernel/cuckoo_hash/print'
terminate_command = 'sudo rmmod cuckoo_hash_kernel'

for command in initial_commands:
    subprocess.call(command, shell=True)

print('Command executed with their associated terminal:')
executed_commands = []
with ThreadPoolExecutor() as executor:
    for i in range(NUM_TERMINALS):
        executed_commands.append(executor.submit(execute_commands, executing_commands[i], i))

for command in executed_commands:
    result = command.result()

subprocess.call(print_command, shell=True)

# Check that the solution is possible
words_value = []
for word in words:
    command = 'echo "' + str(word) + '" | sudo tee /sys/kernel/cuckoo_hash/get>/dev/null'
    subprocess.call(command, shell=True)

# At the beginning no words have value
possible_responses = []
for word in words:
    possible_responses_t = []
    for t in range(NUM_TERMINALS):
        resp = 'could not find [' + word + ']'
        for operation in reversed(operations[t]):
            if(operation[0] == 'delete' and operation[1] == word):
                break
            elif operation[0] == 'insert' and operation[1] == word:
                resp = 'found [' + operation[1] + '] value=' + str(operation[2])
                break
        possible_responses_t.append(resp)
    possible_responses.append(possible_responses_t)

subprocess.call(terminate_command, shell=True)
output_bytes = subprocess.check_output(["sudo", "dmesg"])
output_string = output_bytes.decode("utf-8")
lines = output_string.split("\n")
lines = lines[::-1]
responses = []

for line in lines:
    if 'Start' in line:
        break
    responses.append(line[15::])

print('Responses taken from the messages of the kernel:')
responses = responses[::-1]
for response in responses[:len(responses) - NUM_WORDS - 1]:
    print(response)

print('Corresponding value of all the words:')
word_index = 0
correct = True
for response in responses[len(responses) - NUM_WORDS - 1: len(responses) - 1]:
    print(response)
    if response not in possible_responses[word_index]:
        correct = False
    word_index += 1
if correct:
    print('The result is possible')
else:
    print('The result is not possible')