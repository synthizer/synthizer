This is an HRTF processor.  It spits out a bunch of static data tables from the MIT dataset.  You probably don't need to run this yourself unless you want to change the HRTF processing pipeline.  If you do:

```
cdd hrtf_data
pip install -r requirements.txt
python main.py
```

Which will update a couple files in include and src for you.

If you're updating the dataset all the time, you can do i.e. `ninja hrtf` followed by `ninja` to build the hrtf dataset followed by the library, and if you want to always process hrtf data you can turn on the CMake option `SYNTHIZER_PROCESS_HRTF` which will make this target a dependency of the main synthizer target.

Note that CMake isn't smart enough to update your shell appropriately if you enable the above option.
