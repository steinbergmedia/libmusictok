# Contributing to LibMusicTok

First off, thank you for considering contributing to our project. Your help is essential for keeping it great.

## Reporting Issues

If you find a bug, a typo, or a documentation mistake, please report it by
[creating a new issue](https://github.com/steinbergmedia/libmusictok/issues).
When creating an issue, please:

- Use a clear and descriptive title.
- Include as much relevant information as possible.
- If applicable, provide steps to reproduce the issue.

## Submitting Changes

Before making any changes, please [create an issue](https://github.com/steinbergmedia/libmusictok/issues)
to discuss the changes you wish to make.
This will ensure that your work is aligned with the project's goals and that it hasn't already been addressed.

### Forking the Repository

1. Fork the repository by clicking the "Fork" button at the top right of the repository page.
2. Clone your forked repository:
    ```sh
    git clone https://github.com/your-username/project-name.git
    cd project-name
    ```
3. Add the original repository as a remote:
    ```sh
    git remote add upstream https://github.com/steinbergmedia/libmusictok.git
    ```

### Creating a Branch

1. Create a new branch for your changes:
    ```sh
    git checkout -b your-branch-name
    ```

### Making Changes

Make your changes to the code, documentation, or other resources. Follow the coding standards
and commit your changes with clear and concise commit messages.

### Running Tests

Make sure to run the tests before submitting your changes by building and running the LibMusicTokTest target.
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DLIBMUSICTOK_BUILD_TESTS=ON -DLIBMUSICTOK_BUILD_BENCHMARKS=ON
cmake --build build --config Release
./build/LibMusicTokTest
```
Also test the docker image, which has the test target built-in and ready to run:

```sh
docker build -t libmusictok-ubuntu .
docker run libmusictok-ubuntu
```

Make sure your changes adhere to the requirements of the project as found in the main README.md.

### Pushing Changes

1. Push your changes to your forked repository:
    ```sh
    git push origin your_branch_name
    ```
2. Create a pull request from your branch.
Be sure to include a clear and descriptive title and description of your changes.

## Style Guides

### Commit Messages

- Use the present tense ("Add feature" not "Added feature").
- Use the imperative mood ("Move cursor to..." not "Moves cursor to...").
- Limit the first line to 72 characters or less.
- Reference issues and pull requests liberally after the first line.

### Code Style

- Follow the coding conventions used in the project.
- Write tests for your code if applicable.

### Code Formatting:

Format your code using the .clang-format file provided. To do so:

- Install [clang-format](https://clang.llvm.org/docs/ClangFormat.html).
- Run the following:
```sh
clang-format -i <filename> <otherFilename> <moreFilenames>
```

If you plan to work on this repository often, I recommend adding this script to your git pre-commit hook to format the files automatically at each commit for all modified files.

```bash
#!/bin/bash

# Find all staged C/C++ files
files=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|hpp|cc|c|h)$')

# Check if there are any C/C++ files
if [ -z "$files" ]; then
    exit 0
fi

# Format each file
echo "Running clang-format on staged files..."
for file in $files; do
    clang-format -i "$file"

# Add changes made by clang-format to staging
    git add "$file"
done

echo "Clang-format applied."

exit 0
```

## Getting Help

If you need help or have a question, please reach out by
[creating an issue](https://github.com/steinbergmedia/libmusictok/issues).

Thank you for your contributions!
