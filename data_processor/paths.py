import os.path

processor_path = os.path.split(os.path.abspath(__file__))[0]
data_path = os.path.join(processor_path, "mit_kemar")
repo_path = os.path.abspath(os.path.join(processor_path, ".."))
