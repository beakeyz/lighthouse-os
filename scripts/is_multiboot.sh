
result=$(grub-file --is-x86-multiboot ./../lightos.elf)

echo "$result"

if [ "$result" == "1" ]; then
    echo "yea"
else
    echo "nah"
fi
