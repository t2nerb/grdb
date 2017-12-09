Create Ubuntu container with dependencies for PA3
-------------------------------------------------

This image is built off of `ubuntu:latest` image. I.E Ubuntu 16.04. All commands are intended to be ran from the project's root directory.

1. Place the Dockerfile in the main directory for the project.

2. Execute the following command to build the image with necessary dependencies: `docker build -t csci3287:latest . `

3. After creating the image with necessary dependencies, use the following command to create a container and mount the project directory in the container: ` docker run -d --name csci3287 -it -v $(PWD):/root/Code/ csci3287`

4. Now that the container is running, let's access it by starting a new bash process: `docker exec -it csci3287 bash`

5. From here, you should be in `/root/` directory, and all of your code should be in `/root/Code`.

Now, you have a container with all the shit you'd normally run your VM in.

Managing your images and containers
----------------------

For the above, your container name and image name will be `csci3287`.

To stop the container: `docker stop <container-name>`

To restart the container: `docker start <container-name>`

To check the status of all containers: `docker ps -a`

To remove a container: `docker rm <container-name>`

To remove the image: `docker rmi <image-name>`
