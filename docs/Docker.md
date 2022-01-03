# Docker mode

## Prerequisites:
- Hardware requirements: 
  CPU must contain **SGX module**, and make sure the SGX function is turned on in the bios, please click this page to check if your machine supports SGX
  
- Other configurations
  - **Secure Boot** in BIOS needs to be turned off
  - Need use ordinary account, **cannot support root account**

- Ensure that you have one of the following required operating systems:
  * Ubuntu\* 16.04 LTS Desktop 64bits
  * Ubuntu\* 16.04 LTS Server 64bits
  * Ubuntu\* 18.04 LTS Desktop 64bits
  * Ubuntu\* 18.04 LTS Server 64bits

- Install git-lfs:
  ```
  curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | sudo bash
  sudo apt-get install git-lfs
  git lfs install
  ```

- Install docker (for docker mode):
  ```
  sudo apt-get update
  curl -fsSL https://get.docker.com | bash -s docker --mirror Aliyun
  ```

- Clone project
  ```
  git clone https://github.com/mannheim-network/spacex-storage.git
  ```

## Build
- Build spacex storage docker.
  ```
  sudo ./docker/build.sh
  ```
