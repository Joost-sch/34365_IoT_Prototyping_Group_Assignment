import re

# ==============================
# Define settings
# ==============================

# File paths
input_file = r"C:/Users/fbieh/OneDrive - Danmarks Tekniske Universitet/34365 - IoT prototyping/happy.xbm"
output_file = r"C:/Users/fbieh/OneDrive - Danmarks Tekniske Universitet/34365 - IoT prototyping/happy_inverted.txt"

new_rows = 96
new_cols = 12

# Define new matrix column order (1-based indexing)
column_order = [12,11,10,9,8,7,6,5,4,3,2,1]


# ==============================
# Read values from input TXT
# ==============================

with open(input_file, "r", encoding="utf-8-sig") as file:
    content = file.read()

# Find the start and end of the data section
start = content.find("{")
end = content.find("}", start)

if start == -1 or end == -1:
    raise ValueError("Could not find '= {' or '};' in input file")

# Extract the content between the markers
matrix_content = content[start + 3:end]

# Extract hexadecimal values as strings
numbers = re.findall(
    r'0[xX][0-9a-fA-F]+',
    matrix_content
)

# Diagnostic output
print("Extracted data section:")
print(matrix_content)

print("\nNumber of values found:", len(numbers))
print("Extracted values:", numbers)

if not numbers:
    raise ValueError("No hexadecimal values found in the input file")



# ==============================
# Create new matrix
# ==============================

total_numbers = new_rows * new_cols

numbers = numbers[:total_numbers]

# Fill remaining positions with empty strings
numbers += [""] * (total_numbers - len(numbers))

new_matrix = [
    numbers[i:i + new_cols]
    for i in range(0, total_numbers, new_cols)
]


# ==============================
# Reorder columns in new matrix
# ==============================

new_matrix = [
    [row[col - 1] for col in column_order]
    for row in new_matrix
]


# ==============================
# Save new matrix to TXT file
# ==============================

with open(output_file, "w") as file:
    for row in new_matrix:
        file.write(", ".join(map(str, row)) + ",\n")


# ==============================
# Print result
# ==============================

print("New matrix:")
for row in new_matrix:
    print(row)

print(f"\nMatrix saved to: {output_file}")