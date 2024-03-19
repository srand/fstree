fstree - filesystem tree sharing and sync
=========================================

Fstree a tool to share a filesystem tree over the network and keep it in sync.

It is similar to rsync, but it is designed to be used in a client-server model, where
trees are pushed and pulled between the server and the clients. Merkle trees are used
to efficiently determine what parts of the tree need to be downloaded or uploaded.

Files and directories can be ignored using a `.fstreeignore` file in the root of the tree.
It uses the same syntax as `.gitignore` files.

A simple gRPC protocol is used to communicate between the client and the server.
A reference server implementation is available as the
`robrt/jolt-cache <https://hub.docker.com/r/robrt/jolt-cache>`_
Docker image on Docker Hub.

Usage
-----

To start the server, run:

.. code-block::

  docker run -v /path/to/data:/data -p 9090:9090 robrt/jolt-cache -i

To push a tree to the server, run:

.. code-block::

  fstree write-tree-push --remote tcp://localhost:9090 /path/to/data

The steps above can be split into two separate commands:

.. code-block::

  fstree write-tree /path/to/data
  fstree push --remote tcp://localhost:9090 <digest>

To pull a tree from the server, run:

.. code-block::

  fstree pull-checkout --remote tcp://localhost:9090 <digest> /path/to/data

The steps above can be split into two separate commands:

.. code-block::

  fstree pull --remote tcp://localhost:9090 <digest>
  fstree checkout <digest> /path/to/data

To list the contents of a tree, run:

.. code-block::

  fstree ls-tree <digest>


Configuration
-------------

Configuration can be passed as command line arguments or environment variables.

The following environment variables are supported:

- ``FSTREE_CACHE``: The directory where the local object cache is stored. Defaults to ``~/.cache/fstree`` on Linux and macOS and ``~/AppData/Local/fstree`` on Windows.
- ``FSTREE_IGNORE``: The relative path to the ignore file. Defaults to ``.fstreeignore`` in the root of the tree.
- ``FSTREE_REMOTE``: The remote address of the server to connect to. Defaults to ``tcp://localhost:9090``.
- ``FSTREE_THREADS``: The number of threads to use for parallel operations. Defaults to the number of CPU cores.

The following command line arguments are supported:

- ``--cache``: See ``FSTREE_CACHE``.
- ``--ignore``: See ``FSTREE_IGNORE``.
- ``--remote``: See ``FSTREE_REMOTE``.
- ``--threads``: See ``FSTREE_THREADS``.


Building
--------

The project can be built using CMake, but developers typically use Jolt as a higher level orchestrator.
To build the project using Jolt, run:

.. code-block::

  pip install jolt
  jolt build fstree

