# :fontawesome-solid-gear: Configuration


To start the configuration, please run:

`vdb-config -i`

You will see a screen where you operate the buttons by pressing the letter highlighted in red, or by pressing the tab-key until the wanted button is reached and then pressing the space- or the enter-key.

1. You want to enable the "Remote Access" option on the Main screen.
2. If you would like the toolkit to default to using the smaller [SRA Lite](https://www.ncbi.nlm.nih.gov/sra/docs/sra-data-formats/) format with simplified quality scores, set the "Prefer SRA Lite files with simplified base quality scores" option on the Main screen.
3. Proceed to the "Cache" tab where you will want to enable "local file-caching" and you want to set the "Location of user-repository".

      a) The repository directory needs to be set to an empty folder. This is the folder where prefetch will deposit the files. 

4. Go to your cloud provider tab and accept to "report cloud instance identity".

!!! info
      The cloud instance identity only reports back in what cloud (AWS v GCP) you are working so you can access data for free.

You may now use the toolkit as you always had. Should you experience any problems please email sra-tools@ncbi.nlm.nih.gov