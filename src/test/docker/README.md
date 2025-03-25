### Docker images for SCR

The Dockerfiles, resulting docker images, and `docker-run-checks.sh`
script contained herein are used as part of the strategy for CI testing
of SCR.

Docker is used under CI to speed up deployment of an
environment with correct build dependencies and to keep a docker
image deployed at `scr/SCR` DockerHub with latest master build
(`scr/SCR:latest`) and tagged builds (`scr/SCR:v<tag>`),
which can be used by other projects to build against the latest
or a tagged version of SCR.

#### SCR/testenv Docker images

The Dockerfiles `jammy/Dockerfile`, `focal/Dockerfile`,
`el7/Dockerfile`, and `el8/Dockerfile` describe the images built
under the `scr/testenv:jammy`, `scr/testenv:focal`,
`scr/testenv:el7`, and `scr/testenv:el8` respectively, and
include the base dependencies required to build SCR. These images
are updated manually by SCR maintainers, but the Dockerfiles should
be kept up to date for a single point of management.

#### The "checks" build Dockerfile

A secondary Dockerfile exists under `./checks/Dockerfile` which is used
to customize the `scr/testenv` before building. Without this secondary
`docker build` stage, there would be no way for PRs on GitHub to add
new dependencies for users that are not core maintainers (or the "base"
images would need to be completely rebuilt on each CI run).

#### Adding a new dependency

When constructing a PR that adds new dependency, the dependency should
be added (for both rh/el and Ubuntu) in `checks/Dockerfile`. This will
result in a temporary docker image being created during testing of the
PR with the dependency installed.

Later, a SCR maintainer can move the dependency into the `testenv`
Docker images `jammy/Dockerfile` and `el7/Dockerfile`.
These docker images should then be built by hand and manually
pushed to DockerHub at `scr/testenv:jammy` and
`scr/testenv:el7`. Be sure to test that the `docker-run-test.sh`
script still runs against the new `testenv` images, e.g.:

```
$ for i in focal el7 el8 fedora33 fedora34 fedora35 fedora38; do
    make clean &&
    docker build --no-cache -t scr/testenv:$i src/test/docker/$i &&
    src/test/docker/docker-run-checks.sh -j 4 --image=$i &&
    docker push scr/testenv:$i
  done
```

#### Bookworm and Jammy multiarch images

Building the images for linux/amd64, linux/arm64 and linux/386 requires the
Docker buildx extensions, see

 https://www.docker.com/blog/multi-arch-build-and-images-the-simple-way/

and run
```
$  docker buildx build --push --platform=linux/arm64,linux/amd64 --tag scr/testenv:jammy -f src/test/docker/jammy/Dockerfile .
$  docker buildx build --push --platform=linux/386,linux/amd64,linux/arm64 --tag scr/testenv:bookworm -f src/test/docker/bookworm/Dockerfile .
```

to build and push images to docker hub.

#### Local Testing

Developers can test the docker images themselves. If new dependencies are needed,
they can update the `$image` Dockerfiles manually (where `$image` is one of jammy, el7, el8, or focal).
To create a local Docker image, run the command:

```
docker build -t scr/testenv:$image src/test/docker/$image
```

To test the locally created image, run:

```
src/test/docker/docker-run-checks.sh -i $image [options] -- [arguments]
```
