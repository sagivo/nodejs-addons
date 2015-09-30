{
  "targets": [
    {
      "target_name": "addon",
      "sources": [ "hello.cc" ],
      "include_dirs" : ["<!(node -e \"require('nan')\")"],
    }
  ]
}