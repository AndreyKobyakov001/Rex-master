To run the scripts for the first time, the container that the script uses must be removed:

```
docker stop containername
docker rm containername
```

To re-run the scripts if they haven't been changed, simply run the existing container generated by those scripts:
```
docker start importcsv
# or if you want to attach to it and see the output
docker start -a importcsv
```

**Note:** some scripts will need to be modified with appropriate credentials/database names.
