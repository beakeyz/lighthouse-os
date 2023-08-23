
result=$(grub-file --is-x86-multiboot ./../aniva.elf)

echo "$result"

if [ "$result" == "1" ]; then
    echo "yea"
else
    echo "nah"
fi
