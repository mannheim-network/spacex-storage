# Spacex Storage [![GitHub license](https://img.shields.io/github/license/mannheim-network/spacex-storage)](LICENSE)
Spacex Storage is an offchain storage work inspector of Mannheim Network running inside TEE enclave.


## Prerequisites:
- Hardware requirements: 
  CPU must contain **SGX module**, and make sure the SGX function is turned on in the bios.
  
- Other configurations
  - **Secure Boot** in BIOS needs to be turned off
  - Need use ordinary account, **cannot support root account**

- Ensure that you have one of the following required operating systems:
  * Ubuntu\* 16.04 LTS Desktop 64bits (just for docker mode)
  * Ubuntu\* 16.04 LTS Server 64bits (just for docker mode)
  * Ubuntu\* 18.04 LTS Desktop 64bits 
  * Ubuntu\* 18.04 LTS Server 64bits 

- Install git-lfs:
  ```
  curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | sudo bash
  sudo apt-get install git-lfs
  git lfs install
  ```

- Clone project
  ```
  git clone https://github.com/mannheim-network/spacex-storage.git
  ```

## Build

### Build from docker
Please refer to [spacex-storage docker mode](docs/Docker.md)

## Run

```
docker-compose -f ./docker/docker-compose.yaml up -d spacex-storage
```

## Contribution
Thank you for considering to help out with the source code! We welcome contributions from anyone on the internet, and are grateful for even the smallest of fixes!
If you'd like to contribute to Mannheim Network, please **fork, fix, commit and send a pull request for the maintainers to review and merge into the main codebase**.

### Rules
Please make sure your contribution adhere to our coding guideliness:
- **No --force pushes** or modifying the master branch history in any way. If you need to rebase, ensure you do it in your own repo.
- Pull requests need to be based on and opened against the `master branch`.
- A pull-request **must not be merged until CI** has finished successfully.
- Make sure your every `commit` is [signed](https://help.github.com/en/github/authenticating-to-github/about-commit-signature-verification)

### Merge process
Merging pull requests once CI is successful:
- A PR needs to be reviewed and approved by project maintainers;
- PRs that break the external API must be tagged with [`breaksapi`](https://github.com/mannheim-network/spacex-storage/labels/breakapi);
- No PR should be merged until **all reviews' comments** are addressed.

## License
[GPL v3](LICENSE)
